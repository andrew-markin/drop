#ifndef LOCALNETWORK_H
#define LOCALNETWORK_H

#include <QHostAddress>
#include <QList>
#include <QNetworkInterface>
#include <QString>

// Finds the address other devices of the local network can reach this computer at
namespace LocalNetwork {

struct Candidate
{
    QString name;
    QString humanReadableName;
    QNetworkInterface::InterfaceType type = QNetworkInterface::Unknown;
    QNetworkInterface::InterfaceFlags flags;
    QHostAddress address;
};

QList<Candidate> getAllCandidates();
QHostAddress probeRouteAddress();

QList<Candidate> rankCandidates(const QList<Candidate>& candidates, const QHostAddress& routeAddress);
QList<Candidate> getCandidates();

bool isPrivateAddress(const QHostAddress& address);
bool looksVirtual(const Candidate& candidate);

QString makeLink(const QHostAddress& address, quint16 port, const QByteArray& secret);

} // namespace LocalNetwork

#endif // LOCALNETWORK_H
