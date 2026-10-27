#include "LocalNetwork.h"

#include <QRegularExpression>
#include <QUdpSocket>

#include <algorithm>

namespace {

static const int RouteProbeTimeout = 100; // 100ms

// Any address outside the local network does, connecting a UDP socket sends nothing anyway
static const QHostAddress RouteProbeAddress("192.0.2.1"); // TEST-NET-1, RFC 5737
static const quint16 RouteProbePort = 9; // Discard

int getScore(const LocalNetwork::Candidate& candidate, const QHostAddress& routeAddress)
{
    int score = 0;

    // The interface the system sends to the outside world through is the most likely to be
    // the one phones are connected to as well
    if (!routeAddress.isNull() && candidate.address.isEqual(routeAddress))
    {
        score += 100;
    }

    if (!LocalNetwork::looksVirtual(candidate))
    {
        score += 50;
    }

    if (candidate.type == QNetworkInterface::Wifi || candidate.type == QNetworkInterface::Ethernet)
    {
        score += 10;
    }

    return score;
}

} // namespace

namespace LocalNetwork {

QList<Candidate> getAllCandidates()
{
    QList<Candidate> candidates;

    for (const QNetworkInterface& networkInterface : QNetworkInterface::allInterfaces())
    {
        for (const QNetworkAddressEntry& entry : networkInterface.addressEntries())
        {
            Candidate candidate;
            candidate.name = networkInterface.name();
            candidate.humanReadableName = networkInterface.humanReadableName();
            candidate.type = networkInterface.type();
            candidate.flags = networkInterface.flags();
            candidate.address = entry.ip();
            candidates.append(candidate);
        }
    }

    return candidates;
}

QHostAddress probeRouteAddress()
{
    QUdpSocket socket;
    socket.connectToHost(RouteProbeAddress, RouteProbePort);

    if (!socket.waitForConnected(RouteProbeTimeout))
    {
        return QHostAddress(); // No default route, the network may still be fine
    }

    return socket.localAddress();
}

QList<Candidate> rankCandidates(const QList<Candidate>& candidates, const QHostAddress& routeAddress)
{
    QList<Candidate> result;

    for (const Candidate& candidate : candidates)
    {
        QNetworkInterface::InterfaceFlags requiredFlags = QNetworkInterface::IsUp | QNetworkInterface::IsRunning;

        if ((candidate.flags & requiredFlags) != requiredFlags ||
            candidate.flags.testFlag(QNetworkInterface::IsLoopBack) ||
            candidate.flags.testFlag(QNetworkInterface::IsPointToPoint) || !isPrivateAddress(candidate.address))
        {
            continue;
        }

        result.append(candidate);
    }

    // Stable, so the system order decides between equal candidates
    std::stable_sort(result.begin(), result.end(), [&routeAddress](const Candidate& a, const Candidate& b) {
        return getScore(a, routeAddress) > getScore(b, routeAddress);
    });

    return result;
}

QList<Candidate> getCandidates()
{
    return rankCandidates(getAllCandidates(), probeRouteAddress());
}

bool isPrivateAddress(const QHostAddress& address)
{
    // IPv4 only: phones on a home network always have it, and IPv6 links are long and change often
    if (address.protocol() != QAbstractSocket::IPv4Protocol)
    {
        return false;
    }

    static const QList<QPair<QHostAddress, int>> privateSubnets = {
        QHostAddress::parseSubnet("10.0.0.0/8"),
        QHostAddress::parseSubnet("172.16.0.0/12"),
        QHostAddress::parseSubnet("192.168.0.0/16"),
    };

    for (const QPair<QHostAddress, int>& subnet : privateSubnets)
    {
        if (address.isInSubnet(subnet))
        {
            return true;
        }
    }

    return false;
}

bool looksVirtual(const Candidate& candidate)
{
    if (candidate.type == QNetworkInterface::Virtual)
    {
        return true;
    }

    // Containers, virtual machines and VPNs. A bare "br0" is left alone: it is often a bridge to
    // the real network, while Docker names its bridges "br-" followed by the network identifier.
    static const QRegularExpression namePattern(
        "^(docker|br-[0-9a-f]{12}|virbr|veth|vmnet|vboxnet|lxc|lxd|cni|flannel|tun|tap|wg|zt|tailscale|utun)",
        QRegularExpression::CaseInsensitiveOption);

    // Windows names adapters for people, not by kind
    static const QRegularExpression humanReadablePattern("vEthernet|VirtualBox|VMware|Hyper-V|WSL|VPN",
                                                         QRegularExpression::CaseInsensitiveOption);

    return namePattern.match(candidate.name).hasMatch() ||
           humanReadablePattern.match(candidate.humanReadableName).hasMatch();
}

QString makeLink(const QHostAddress& address, quint16 port, const QByteArray& secret)
{
    QByteArray encodedSecret = secret.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QString("http://%1:%2/#k=%3").arg(address.toString()).arg(port).arg(QString::fromLatin1(encodedSecret));
}

} // namespace LocalNetwork
