#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QAbstractHttpServer>
#include <QHttpServerResponder>
#include <QJsonObject>
#include <QHostAddress>

class QTcpServer;
class Uploads;

// Serves the web client from the application resources to the local network
class WebServer : public QAbstractHttpServer
{
    Q_OBJECT

public:
    WebServer(const QString& resourceRoot, Uploads* uploads, QObject* parent = nullptr);

    bool start(const QHostAddress& address, quint16 port = 0);

    QString errorString() const;
    QHostAddress serverAddress() const;
    quint16 serverPort() const;

protected:
    bool handleRequest(const QHttpServerRequest& request, QHttpServerResponder& responder) override;
    void missingHandler(const QHttpServerRequest& request, QHttpServerResponder& responder) override;

private:
    void respondWithFile(const QHttpServerRequest& request, QHttpServerResponder& responder);
    void respondToApi(const QHttpServerRequest& request, QHttpServerResponder& responder);

    QByteArray openSession();
    bool touchSession(const QByteArray& sessionId);

    QJsonObject announceFile(const QByteArray& body, QHttpServerResponder::StatusCode* status);
    QJsonObject writeChunk(const QByteArray& fileId, const QString& index, const QByteArray& body,
                           QHttpServerResponder::StatusCode* status);
    QJsonObject getStatus();

private:
    QString m_resourceRoot;
    Uploads* m_uploads;
    QList<QByteArray> m_sessionIds; // Most recently used last
    QTcpServer* m_tcpServer;
};

#endif // WEBSERVER_H
