#ifndef RAWHTTP_H
#define RAWHTTP_H

#include <QDeadlineTimer>
#include <QMap>
#include <QTcpSocket>
#include <QTest>

// Plain HTTP/1.1 requests, so tests control the exact path, headers and body
namespace RawHttp {

static const int ResponseTimeout = 5000; // 5s

struct Response
{
    int status = 0;
    QMap<QByteArray, QByteArray> headers;
    QByteArray body;
};

inline Response send(quint16 port, const QByteArray& method, const QByteArray& path, const QByteArray& host,
                     const QByteArray& body = QByteArray())
{
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, port);

    Response response;

    if (!socket.waitForConnected(ResponseTimeout))
    {
        return response;
    }

    QByteArray request = method + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n";

    if (!body.isEmpty() || method == "POST")
    {
        request += "Content-Type: application/octet-stream\r\nContent-Length: " + QByteArray::number(body.size()) +
                   "\r\n";
    }

    socket.write(request + "\r\n" + body);

    QByteArray data;

    // The server runs in this thread, so wait with the event loop running
    QDeadlineTimer deadline(ResponseTimeout);

    while (socket.state() != QAbstractSocket::UnconnectedState && !deadline.hasExpired())
    {
        QTest::qWait(5);
        data.append(socket.readAll());
    }

    data.append(socket.readAll());

    int headerEnd = data.indexOf("\r\n\r\n");

    if (headerEnd < 0)
    {
        return response;
    }

    QList<QByteArray> lines = data.left(headerEnd).split('\n');
    response.status = lines.takeFirst().split(' ').value(1).toInt();

    for (const QByteArray& line : lines)
    {
        int colon = line.indexOf(':');
        response.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    response.body = data.mid(headerEnd + 4);
    return response;
}

} // namespace RawHttp

#endif // RAWHTTP_H
