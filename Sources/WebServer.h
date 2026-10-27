#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QAbstractHttpServer>
#include <QHostAddress>

class QTcpServer;

// Serves the web client from the application resources to the local network
class WebServer : public QAbstractHttpServer
{
    Q_OBJECT

public:
    WebServer(const QString& resourceRoot, QObject* parent = nullptr);

    bool start(const QHostAddress& address, quint16 port = 0);

    QString errorString() const;
    QHostAddress serverAddress() const;
    quint16 serverPort() const;

protected:
    bool handleRequest(const QHttpServerRequest& request, QHttpServerResponder& responder) override;
    void missingHandler(const QHttpServerRequest& request, QHttpServerResponder& responder) override;

private:
    void respondWithFile(const QHttpServerRequest& request, QHttpServerResponder& responder);

private:
    QString m_resourceRoot;
    QTcpServer* m_tcpServer;
};

#endif // WEBSERVER_H
