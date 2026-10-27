#include "WebServer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpHeaders>
#include <QHttpServerConfiguration>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QTcpServer>

namespace {

static const QByteArray IndexPath = "/index.html";

// Vite puts the bundles with content hashes in their names here, they never change
static const QString ImmutablePathPrefix = "/assets/";

static const QHash<QString, QByteArray> ContentTypes = {
    {"css", "text/css; charset=utf-8"},
    {"html", "text/html; charset=utf-8"},
    {"ico", "image/x-icon"},
    {"js", "text/javascript; charset=utf-8"},
    {"json", "application/json"},
    {"mp4", "video/mp4"},
    {"png", "image/png"},
    {"svg", "image/svg+xml"},
    {"ttf", "font/ttf"},
    {"txt", "text/plain; charset=utf-8"},
    {"webm", "video/webm"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
};

// The page talks only to the server it came from. Quasar sets inline styles, and the video that
// keeps the screen on is made in the page.
static const QByteArray ContentSecurityPolicy = "default-src 'self'; "
                                                "style-src 'self' 'unsafe-inline'; "
                                                "img-src 'self' data: blob:; "
                                                "media-src 'self' data: blob:; "
                                                "object-src 'none'; "
                                                "base-uri 'none'; "
                                                "form-action 'none'; "
                                                "frame-ancestors 'none'";

QHttpHeaders makeHeaders()
{
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::ContentSecurityPolicy, ContentSecurityPolicy);
    headers.append(QHttpHeaders::WellKnownHeader::XContentTypeOptions, "nosniff");
    headers.append(QHttpHeaders::WellKnownHeader::XFrameOptions, "DENY");
    headers.append("Referrer-Policy", "no-referrer");
    return headers;
}

void respondWithStatus(QHttpServerResponder& responder, QHttpServerResponder::StatusCode status)
{
    QHttpHeaders headers = makeHeaders();
    headers.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-store");
    responder.write(QByteArray(), headers, status);
}

} // namespace

WebServer::WebServer(const QString& resourceRoot, QObject* parent)
    : QAbstractHttpServer(parent), m_resourceRoot(resourceRoot)
{
    m_tcpServer = new QTcpServer(this);
}

bool WebServer::start(const QHostAddress& address, quint16 port)
{
    if (!m_tcpServer->listen(address, port))
    {
        qWarning() << "Unable to start web server:" << m_tcpServer->errorString();
        return false;
    }

    return bind(m_tcpServer);
}

QString WebServer::errorString() const
{
    return m_tcpServer->errorString();
}

QHostAddress WebServer::serverAddress() const
{
    return m_tcpServer->serverAddress();
}

quint16 WebServer::serverPort() const
{
    return m_tcpServer->serverPort();
}

bool WebServer::handleRequest(const QHttpServerRequest& request, QHttpServerResponder& responder)
{
    // Pages of other sites may point the browser here with their own host name (DNS rebinding),
    // the server answers only to its own address
    QByteArray expectedHost = QString("%1:%2").arg(serverAddress().toString()).arg(serverPort()).toLatin1();

    if (request.headers().value(QHttpHeaders::WellKnownHeader::Host) != expectedHost)
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::MisdirectedRequest);
        return true;
    }

    if (request.method() != QHttpServerRequest::Method::Get)
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::MethodNotAllowed);
        return true;
    }

    respondWithFile(request, responder);
    return true;
}

void WebServer::missingHandler(const QHttpServerRequest& request, QHttpServerResponder& responder)
{
    Q_UNUSED(request);
    respondWithStatus(responder, QHttpServerResponder::StatusCode::NotFound);
}

void WebServer::respondWithFile(const QHttpServerRequest& request, QHttpServerResponder& responder)
{
    QString path = request.url().path();

    if (path == "/")
    {
        path = IndexPath;
    }

    // Resources have no symbolic links, so a clean path that stays at the root stays inside it
    QString cleanPath = QDir::cleanPath(path);

    if (!cleanPath.startsWith('/') || cleanPath.startsWith("/..") || cleanPath != path)
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::NotFound);
        return;
    }

    QFile file(m_resourceRoot + cleanPath);

    if (QFileInfo(file).isDir() || !file.open(QIODevice::ReadOnly))
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::NotFound);
        return;
    }

    QHttpHeaders headers = makeHeaders();
    QString suffix = QFileInfo(cleanPath).suffix().toLower();
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, ContentTypes.value(suffix, "application/octet-stream"));

    if (cleanPath.startsWith(ImmutablePathPrefix))
    {
        headers.append(QHttpHeaders::WellKnownHeader::CacheControl, "public, max-age=31536000, immutable");
    }
    else
    {
        // Every start is a new server, the page must not come from an old one
        headers.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-store");
    }

    responder.write(file.readAll(), headers);
}
