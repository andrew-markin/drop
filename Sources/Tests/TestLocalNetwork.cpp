#include <QTest>

#include "LocalNetwork.h"

using LocalNetwork::Candidate;

namespace {

static const QNetworkInterface::InterfaceFlags ActiveFlags =
    QNetworkInterface::IsUp | QNetworkInterface::IsRunning | QNetworkInterface::CanBroadcast;

Candidate makeCandidate(const QString& name, const QString& address,
                        QNetworkInterface::InterfaceType type = QNetworkInterface::Ethernet,
                        QNetworkInterface::InterfaceFlags flags = ActiveFlags,
                        const QString& humanReadableName = QString())
{
    Candidate candidate;
    candidate.name = name;
    candidate.humanReadableName = humanReadableName.isEmpty() ? name : humanReadableName;
    candidate.type = type;
    candidate.flags = flags;
    candidate.address = QHostAddress(address);
    return candidate;
}

QStringList getNames(const QList<Candidate>& candidates)
{
    QStringList names;

    for (const Candidate& candidate : candidates)
    {
        names.append(candidate.name);
    }

    return names;
}

} // namespace

class TestLocalNetwork : public QObject
{
    Q_OBJECT

private slots:
    void isPrivateAddress_data();
    void isPrivateAddress();

    void looksVirtual_data();
    void looksVirtual();

    void rankCandidatesFilters();
    void rankCandidatesPrefersRoute();
    void rankCandidatesPrefersRealInterfaces();
    void rankCandidatesKeepsSystemOrder();

    void getCandidates();

    void makeLink();
};

void TestLocalNetwork::isPrivateAddress_data()
{
    QTest::addColumn<QString>("address");
    QTest::addColumn<bool>("expected");

    QTest::newRow("10/8") << "10.1.2.3" << true;
    QTest::newRow("172.16/12 start") << "172.16.0.1" << true;
    QTest::newRow("172.16/12 end") << "172.31.255.254" << true;
    QTest::newRow("172.32") << "172.32.0.1" << false;
    QTest::newRow("192.168/16") << "192.168.0.5" << true;
    QTest::newRow("public") << "8.8.8.8" << false;
    QTest::newRow("loopback") << "127.0.0.1" << false;
    QTest::newRow("link local") << "169.254.10.20" << false;
    QTest::newRow("carrier grade nat") << "100.64.0.1" << false;
    QTest::newRow("ipv6 unique local") << "fd00::1" << false;
    QTest::newRow("ipv6 link local") << "fe80::1" << false;
}

void TestLocalNetwork::isPrivateAddress()
{
    QFETCH(QString, address);
    QFETCH(bool, expected);

    QCOMPARE(LocalNetwork::isPrivateAddress(QHostAddress(address)), expected);
}

void TestLocalNetwork::looksVirtual_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("humanReadableName");
    QTest::addColumn<int>("type");
    QTest::addColumn<bool>("expected");

    QTest::newRow("ethernet") << "enp3s0" << "" << int(QNetworkInterface::Ethernet) << false;
    QTest::newRow("wifi") << "wlp2s0" << "" << int(QNetworkInterface::Wifi) << false;
    QTest::newRow("real bridge") << "br0" << "" << int(QNetworkInterface::Ethernet) << false;
    QTest::newRow("docker") << "docker0" << "" << int(QNetworkInterface::Ethernet) << true;
    QTest::newRow("docker network") << "br-1a2b3c4d5e6f" << "" << int(QNetworkInterface::Ethernet) << true;
    QTest::newRow("libvirt") << "virbr0" << "" << int(QNetworkInterface::Ethernet) << true;
    QTest::newRow("wireguard") << "wg0" << "" << int(QNetworkInterface::Unknown) << true;
    QTest::newRow("virtual type") << "eth9" << "" << int(QNetworkInterface::Virtual) << true;
    QTest::newRow("windows wifi") << "wireless_32768" << "Wi-Fi" << int(QNetworkInterface::Wifi) << false;
    QTest::newRow("windows wsl") << "ethernet_32777" << "vEthernet (WSL)" << int(QNetworkInterface::Ethernet)
                                 << true;
    QTest::newRow("windows virtualbox") << "ethernet_32778" << "VirtualBox Host-Only Network"
                                        << int(QNetworkInterface::Ethernet) << true;
}

void TestLocalNetwork::looksVirtual()
{
    QFETCH(QString, name);
    QFETCH(QString, humanReadableName);
    QFETCH(int, type);
    QFETCH(bool, expected);

    Candidate candidate = makeCandidate(name, "192.168.0.5", QNetworkInterface::InterfaceType(type), ActiveFlags,
                                        humanReadableName);

    QCOMPARE(LocalNetwork::looksVirtual(candidate), expected);
}

void TestLocalNetwork::rankCandidatesFilters()
{
    QList<Candidate> candidates = {
        makeCandidate("lo", "127.0.0.1", QNetworkInterface::Loopback,
                      ActiveFlags | QNetworkInterface::IsLoopBack),
        makeCandidate("down", "192.168.1.5", QNetworkInterface::Ethernet, QNetworkInterface::IsUp),
        makeCandidate("ppp0", "10.8.0.2", QNetworkInterface::Ppp, ActiveFlags | QNetworkInterface::IsPointToPoint),
        makeCandidate("public", "203.0.113.5"),
        makeCandidate("ipv6", "fd00::5"),
        makeCandidate("wlan0", "192.168.0.5", QNetworkInterface::Wifi),
    };

    QCOMPARE(getNames(LocalNetwork::rankCandidates(candidates, QHostAddress())), QStringList({"wlan0"}));
}

void TestLocalNetwork::rankCandidatesPrefersRoute()
{
    QList<Candidate> candidates = {
        makeCandidate("eth0", "10.0.0.5"),
        makeCandidate("wlan0", "192.168.0.5", QNetworkInterface::Wifi),
    };

    QCOMPARE(getNames(LocalNetwork::rankCandidates(candidates, QHostAddress("192.168.0.5"))),
             QStringList({"wlan0", "eth0"}));
}

void TestLocalNetwork::rankCandidatesPrefersRealInterfaces()
{
    QList<Candidate> candidates = {
        makeCandidate("docker0", "172.17.0.1"),
        makeCandidate("virbr0", "192.168.122.1"),
        makeCandidate("enp3s0", "192.168.0.5"),
    };

    // Without a default route, as on a network with no internet access
    QCOMPARE(getNames(LocalNetwork::rankCandidates(candidates, QHostAddress())),
             QStringList({"enp3s0", "docker0", "virbr0"}));
}

void TestLocalNetwork::rankCandidatesKeepsSystemOrder()
{
    QList<Candidate> candidates = {
        makeCandidate("eth1", "192.168.1.5"),
        makeCandidate("eth0", "192.168.0.5"),
    };

    QCOMPARE(getNames(LocalNetwork::rankCandidates(candidates, QHostAddress())), QStringList({"eth1", "eth0"}));
}

void TestLocalNetwork::getCandidates()
{
    // The real system: whatever it has, only usable addresses come out
    for (const Candidate& candidate : LocalNetwork::getCandidates())
    {
        QVERIFY(LocalNetwork::isPrivateAddress(candidate.address));
        QVERIFY(!candidate.flags.testFlag(QNetworkInterface::IsLoopBack));
    }
}

void TestLocalNetwork::makeLink()
{
    QByteArray secret(32, '\0');

    for (int i = 0; i < secret.size(); i++)
    {
        secret[i] = char(i * 8);
    }

    QCOMPARE(LocalNetwork::makeLink(QHostAddress("192.168.0.5"), 54246, secret),
             "http://192.168.0.5:54246/#k=AAgQGCAoMDhASFBYYGhweICIkJigqLC4wMjQ2ODo8Pg");
}

QTEST_GUILESS_MAIN(TestLocalNetwork)
#include "TestLocalNetwork.moc"
