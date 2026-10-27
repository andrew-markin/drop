#include "WebServer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpHeaders>
#include <QHttpServerConfiguration>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTcpServer>

#include "Uploads.h"

namespace {

static const QByteArray IndexPath = "/index.html";

static const QString ApiPathPrefix = "/api/";
static const QString SessionPath = "/api/session";

static const int SessionIdSize = 16;
static const int SessionCountMax = 64;
static const int FileIdSize = 16;

// Room for a JSON request, sealed
static const qint64 BodySizeReserve = 4096;

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

void respondWithData(QHttpServerResponder& responder, const QByteArray& data, const QByteArray& contentType)
{
    QHttpHeaders headers = makeHeaders();
    headers.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-store");
    headers.append(QHttpHeaders::WellKnownHeader::ContentType, contentType);
    responder.write(data, headers);
}

QByteArray toJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString encodeBase64Url(const QByteArray& data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// Returns an empty array for anything but base64url of exactly the expected size
QByteArray decodeBase64Url(const QString& text, int expectedSize)
{
    auto result = QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::Base64UrlEncoding |
                                                                      QByteArray::AbortOnBase64DecodingErrors);

    if (!result || result.decoded.size() != expectedSize || encodeBase64Url(result.decoded) != text)
    {
        return QByteArray();
    }

    return result.decoded;
}

QHttpServerResponder::StatusCode getStatusCode(Uploads::Error error)
{
    switch (error)
    {
    case Uploads::Error::None:
        return QHttpServerResponder::StatusCode::Ok;
    case Uploads::Error::BadRequest:
        return QHttpServerResponder::StatusCode::BadRequest;
    case Uploads::Error::NotFound:
        return QHttpServerResponder::StatusCode::NotFound;
    case Uploads::Error::NoSpace:
        return QHttpServerResponder::StatusCode::InsufficientStorage;
    case Uploads::Error::Failed:
        break;
    }

    return QHttpServerResponder::StatusCode::InternalServerError;
}

} // namespace

WebServer::WebServer(const QString& resourceRoot, Uploads* uploads, const QByteArray& secret, QObject* parent)
    : QAbstractHttpServer(parent), m_resourceRoot(resourceRoot), m_uploads(uploads), m_secret(secret)
{
    m_tcpServer = new QTcpServer(this);

    QHttpServerConfiguration configuration = this->configuration();
    configuration.setMaximumBodySize((m_uploads ? m_uploads->chunkSize() : 0) + Crypto::SealOverhead + BodySizeReserve);
    setConfiguration(configuration);
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

    if (request.url().path().startsWith(ApiPathPrefix) || request.url().path() == SessionPath)
    {
        respondToApi(request, responder);
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

void WebServer::respondToApi(const QHttpServerRequest& request, QHttpServerResponder& responder)
{
    if (!m_uploads || m_secret.size() != Crypto::SecretSize)
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::NotFound);
        return;
    }

    if (request.method() != QHttpServerRequest::Method::Post)
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::MethodNotAllowed);
        return;
    }

    QString path = request.url().path();
    QHttpServerResponder::StatusCode status = QHttpServerResponder::StatusCode::Ok;

    if (path == SessionPath)
    {
        QJsonObject answer = openSession(request.body(), &status);

        if (status != QHttpServerResponder::StatusCode::Ok)
        {
            respondWithStatus(responder, status);
            return;
        }

        respondWithData(responder, toJson(answer), "application/json");
        return;
    }

    // /api/session/<session>/<operation>[/<arguments>]
    QStringList parts = path.mid(SessionPath.size() + 1).split('/');
    QByteArray sessionId = decodeBase64Url(parts.value(0), SessionIdSize);
    Session session;

    if (!path.startsWith(SessionPath + "/") || sessionId.isEmpty() || !touchSession(sessionId, &session))
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::NotFound);
        return;
    }

    // The path is bound to the request, so a sealed body cannot be replayed to another operation or file
    QByteArray sealedRequest = request.body();
    QByteArray requestAd = Crypto::makeRequestAd(sessionId, "POST", request.url().path(QUrl::FullyEncoded).toLatin1());
    QByteArray body;

    if (!Crypto::open(session.keys.up, sealedRequest, requestAd, &body))
    {
        respondWithStatus(responder, QHttpServerResponder::StatusCode::Unauthorized);
        return;
    }

    QString operation = parts.value(1);
    QJsonObject answer;

    if (operation == "file" && parts.size() == 2)
    {
        answer = announceFile(body, &status);
    }
    else if (operation == "chunk" && parts.size() == 4)
    {
        QByteArray fileId = decodeBase64Url(parts.value(2), FileIdSize);
        answer = writeChunk(fileId, parts.value(3), body, &status);
    }
    else if (operation == "status" && parts.size() == 2)
    {
        answer = getStatus();
    }
    else
    {
        status = QHttpServerResponder::StatusCode::NotFound;
    }

    if (status != QHttpServerResponder::StatusCode::Ok)
    {
        respondWithStatus(responder, status);
        return;
    }

    QByteArray answerAd = Crypto::makeAnswerAd(sealedRequest.left(Crypto::NonceSize));
    respondWithData(responder, Crypto::seal(session.keys.down, toJson(answer), answerAd), "application/octet-stream");
}

QJsonObject WebServer::openSession(const QByteArray& body, QHttpServerResponder::StatusCode* status)
{
    QJsonObject request = QJsonDocument::fromJson(body).object();
    QByteArray clientPublicKey = decodeBase64Url(request.value("key").toString(), Crypto::KeySize);
    QByteArray proof = decodeBase64Url(request.value("proof").toString(), Crypto::KeySize);

    if (clientPublicKey.isEmpty() || proof.isEmpty())
    {
        *status = QHttpServerResponder::StatusCode::BadRequest;
        return QJsonObject();
    }

    // Strangers without the link are turned down before the server does any work for them
    if (!Crypto::equal(proof, Crypto::makeHelloProof(m_secret, clientPublicKey)))
    {
        *status = QHttpServerResponder::StatusCode::Unauthorized;
        return QJsonObject();
    }

    Crypto::KeyPair serverKeyPair = Crypto::generateKeyPair();
    QByteArray shared = Crypto::computeShared(serverKeyPair.secretKey, clientPublicKey);

    if (shared.isEmpty())
    {
        *status = QHttpServerResponder::StatusCode::Unauthorized;
        return QJsonObject();
    }

    Session session;
    session.id = Crypto::randomBytes(SessionIdSize);
    session.keys = Crypto::deriveSessionKeys(m_secret, shared, clientPublicKey, serverKeyPair.publicKey);
    m_sessions.append(session);

    // Pages that were closed never say so, the oldest ones are forgotten
    while (m_sessions.size() > SessionCountMax)
    {
        m_sessions.removeFirst();
    }

    QByteArray confirmAd = Crypto::makeConfirmAd(session.id, clientPublicKey, serverKeyPair.publicKey);

    return QJsonObject({
        {"session", encodeBase64Url(session.id)},
        {"key", encodeBase64Url(serverKeyPair.publicKey)},
        {"proof", encodeBase64Url(Crypto::seal(session.keys.down, QByteArray(), confirmAd))},
    });
}

bool WebServer::touchSession(const QByteArray& sessionId, Session* session)
{
    for (int i = 0; i < m_sessions.size(); i++)
    {
        if (m_sessions.at(i).id == sessionId)
        {
            *session = m_sessions.takeAt(i);
            m_sessions.append(*session);
            return true;
        }
    }

    return false;
}

QJsonObject WebServer::announceFile(const QByteArray& body, QHttpServerResponder::StatusCode* status)
{
    QJsonParseError parseError;
    QJsonObject request = QJsonDocument::fromJson(body, &parseError).object();

    QJsonValue name = request.value("name");
    QJsonValue size = request.value("size");
    QJsonValue modified = request.value("modified");
    QByteArray head = decodeBase64Url(request.value("head").toString(), Uploads::HeadSize);

    if (parseError.error != QJsonParseError::NoError || !name.isString() || !size.isDouble() ||
        !modified.isDouble() || head.isEmpty() || size.toDouble() != double(size.toInteger()) ||
        modified.toDouble() != double(modified.toInteger()))
    {
        *status = QHttpServerResponder::StatusCode::BadRequest;
        return QJsonObject();
    }

    Uploads::Error error;
    QByteArray fileId = m_uploads->announce(name.toString(), size.toInteger(), modified.toInteger(), head, &error);
    *status = getStatusCode(error);

    if (error != Uploads::Error::None)
    {
        return QJsonObject();
    }

    Uploads::Upload upload = m_uploads->upload(fileId);
    QJsonArray received;

    for (int i = 0; i < upload.chunkCount; i++)
    {
        if (upload.received.testBit(i))
        {
            received.append(i);
        }
    }

    return QJsonObject({
        {"file", encodeBase64Url(fileId)},
        {"chunk", m_uploads->chunkSize()},
        {"received", received},
    });
}

QJsonObject WebServer::writeChunk(const QByteArray& fileId, const QString& index, const QByteArray& body,
                                  QHttpServerResponder::StatusCode* status)
{
    static const QRegularExpression indexPattern("^(0|[1-9][0-9]{0,9})$");

    bool indexValid = false;
    int indexValue = indexPattern.match(index).hasMatch() ? index.toInt(&indexValid) : -1;

    if (fileId.isEmpty() || !m_uploads->contains(fileId))
    {
        *status = QHttpServerResponder::StatusCode::NotFound;
        return QJsonObject();
    }

    if (!indexValid)
    {
        *status = QHttpServerResponder::StatusCode::BadRequest;
        return QJsonObject();
    }

    *status = getStatusCode(m_uploads->writeChunk(fileId, indexValue, body));

    Uploads::Upload upload = m_uploads->upload(fileId);
    return QJsonObject({{"received", upload.receivedCount}, {"complete", upload.complete}});
}

QJsonObject WebServer::getStatus()
{
    QJsonArray files;

    for (const Uploads::Upload& upload : m_uploads->uploads())
    {
        files.append(QJsonObject({
            {"file", encodeBase64Url(upload.id)},
            {"name", upload.name},
            {"size", upload.size},
            {"received", upload.receivedCount},
            {"complete", upload.complete},
        }));
    }

    return QJsonObject({{"files", files}});
}
