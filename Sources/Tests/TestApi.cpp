#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "Crypto.h"
#include "RawHttp.h"
#include "Uploads.h"
#include "WebServer.h"

using RawHttp::Response;

namespace {

static const qint64 ChunkSize = 4;

QString encode(const QByteArray& data)
{
    return QString::fromLatin1(data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QJsonObject parse(const QByteArray& data)
{
    return QJsonDocument::fromJson(data).object();
}

QByteArray decode(const QJsonValue& value)
{
    return QByteArray::fromBase64(value.toString().toLatin1(), QByteArray::Base64UrlEncoding);
}

// What the web client does, see docs/PROTOCOL.md
struct Session
{
    QByteArray path; // /api/session/<session>
    QByteArray id;
    Crypto::SessionKeys keys;
};

struct Answer
{
    int status = 0;
    QJsonObject object;
};

QByteArray makeAnnounce(const QString& name, qint64 size, qint64 modified = 0, char headValue = 'h')
{
    QJsonObject object({
        {"name", name},
        {"size", size},
        {"modified", modified},
        {"head", encode(QByteArray(Uploads::HeadSize, headValue))},
    });

    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

class TestApi : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void openSession();
    void openSessionRejectsStrangers_data();
    void openSessionRejectsStrangers();
    void confirmProvesSecret();
    void unknownSession_data();
    void unknownSession();
    void sessionsAreForgotten();
    void onlyPost();

    void uploadFile();
    void resumeUpload();
    void status();

    void announceRejectsBadRequests_data();
    void announceRejectsBadRequests();

    void chunkRejectsBadRequests_data();
    void chunkRejectsBadRequests();

    void bodyTooLarge();

    void rejectsTamperedRequests();
    void rejectsMovedRequests();
    void rejectsOtherSessionKeys();
    void answersAreSealed();

private:
    Response post(const QByteArray& path, const QByteArray& body = QByteArray());
    Response hello(const QByteArray& clientPublicKey, const QByteArray& proof);
    Session openSession(const QByteArray& secret);
    QByteArray openSessionPath();
    Answer call(const Session& session, const QByteArray& operation, const QByteArray& plaintext = QByteArray());
    Answer call(const QByteArray& sessionPath, const QByteArray& plaintext = QByteArray());

private:
    QByteArray m_secret;
    QList<Session> m_openSessions;

private:
    QTemporaryDir* m_temporaryDir = nullptr;
    Uploads* m_uploads = nullptr;
    WebServer* m_server = nullptr;
};

void TestApi::init()
{
    m_temporaryDir = new QTemporaryDir();
    QVERIFY(m_temporaryDir->isValid());

    QVERIFY(Crypto::init());
    m_secret = Crypto::randomBytes(Crypto::SecretSize);
    m_openSessions.clear();

    m_uploads = new Uploads(QDir(m_temporaryDir->path()), ChunkSize);
    m_server = new WebServer(":/Web", m_uploads, m_secret);
    QVERIFY(m_server->start(QHostAddress::LocalHost));
}

void TestApi::cleanup()
{
    delete m_server;
    delete m_uploads;
    delete m_temporaryDir;
}

Response TestApi::post(const QByteArray& path, const QByteArray& body)
{
    QByteArray host = "127.0.0.1:" + QByteArray::number(m_server->serverPort());
    return RawHttp::send(m_server->serverPort(), "POST", path, host, body);
}

Response TestApi::hello(const QByteArray& clientPublicKey, const QByteArray& proof)
{
    QJsonObject request({{"key", encode(clientPublicKey)}, {"proof", encode(proof)}});
    return post("/api/session", QJsonDocument(request).toJson(QJsonDocument::Compact));
}

Session TestApi::openSession(const QByteArray& secret)
{
    Crypto::KeyPair keyPair = Crypto::generateKeyPair();
    Response response = hello(keyPair.publicKey, Crypto::makeHelloProof(secret, keyPair.publicKey));

    Session session;

    if (response.status != 200)
    {
        return session;
    }

    QJsonObject answer = parse(response.body);
    QByteArray serverPublicKey = decode(answer.value("key"));

    session.id = decode(answer.value("session"));
    session.path = "/api/session/" + answer.value("session").toString().toLatin1();
    session.keys = Crypto::deriveSessionKeys(secret, Crypto::computeShared(keyPair.secretKey, serverPublicKey),
                                             keyPair.publicKey, serverPublicKey);

    QByteArray confirm;

    if (!Crypto::open(session.keys.down, decode(answer.value("proof")),
                      Crypto::makeConfirmAd(session.id, keyPair.publicKey, serverPublicKey), &confirm))
    {
        return Session();
    }

    return session;
}

QByteArray TestApi::openSessionPath()
{
    Session session = openSession(m_secret);
    m_openSessions.append(session);
    return session.path;
}

Answer TestApi::call(const QByteArray& sessionPath, const QByteArray& plaintext)
{
    for (const Session& session : m_openSessions)
    {
        if (sessionPath.startsWith(session.path + "/"))
        {
            return call(session, sessionPath.mid(session.path.size() + 1), plaintext);
        }
    }

    // A session the server does not know: any keys do, it must not get as far as opening
    Session unknown;
    unknown.path = sessionPath.left(sessionPath.lastIndexOf('/'));
    unknown.keys = Crypto::SessionKeys{Crypto::randomBytes(Crypto::KeySize), Crypto::randomBytes(Crypto::KeySize)};
    return call(unknown, sessionPath.mid(unknown.path.size() + 1), plaintext);
}

Answer TestApi::call(const Session& session, const QByteArray& operation, const QByteArray& plaintext)
{
    QByteArray path = session.path + "/" + operation;
    QByteArray sealed = Crypto::seal(session.keys.up, plaintext, Crypto::makeRequestAd(session.id, "POST", path));
    Response response = post(path, sealed);

    Answer answer;
    answer.status = response.status;

    if (response.status == 200)
    {
        QByteArray opened;

        if (!Crypto::open(session.keys.down, response.body, Crypto::makeAnswerAd(sealed.left(Crypto::NonceSize)),
                          &opened))
        {
            answer.status = -1; // An answer the client cannot trust
        }

        answer.object = parse(opened);
    }

    return answer;
}

void TestApi::openSession()
{
    Crypto::KeyPair keyPair = Crypto::generateKeyPair();
    Response response = hello(keyPair.publicKey, Crypto::makeHelloProof(m_secret, keyPair.publicKey));
    QCOMPARE(response.status, 200);
    QCOMPARE(response.headers.value("cache-control"), "no-store");

    QJsonObject answer = parse(response.body);
    QCOMPARE(decode(answer.value("session")).size(), 16);
    QCOMPARE(decode(answer.value("key")).size(), Crypto::KeySize);
    QCOMPARE(decode(answer.value("proof")).size(), Crypto::SealOverhead);

    // Every page gets a session and keys of its own
    Session first = openSession(m_secret);
    Session second = openSession(m_secret);
    QVERIFY(!first.id.isEmpty());
    QVERIFY(first.id != second.id);
    QVERIFY(first.keys.up != second.keys.up);
    QVERIFY(first.keys.up != first.keys.down);

    QCOMPARE(call(first, "status").status, 200);
}

void TestApi::openSessionRejectsStrangers_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("status");

    Crypto::KeyPair keyPair = Crypto::generateKeyPair();
    QByteArray key = encode(keyPair.publicKey).toLatin1();
    QByteArray otherProof = encode(Crypto::makeHelloProof(Crypto::randomBytes(32), keyPair.publicKey)).toLatin1();
    QByteArray zeroKey = encode(QByteArray(32, '\0')).toLatin1();
    QByteArray zeroKeyProof = encode(Crypto::makeHelloProof(QByteArray(), QByteArray())).toLatin1();

    QTest::newRow("empty") << QByteArray() << 400;
    QTest::newRow("not json") << QByteArray("hello") << 400;
    QTest::newRow("no proof") << QByteArray("{\"key\":\"" + key + "\"}") << 400;
    QTest::newRow("short key") << QByteArray("{\"key\":\"AAAA\",\"proof\":\"" + otherProof + "\"}") << 400;
    QTest::newRow("other secret") << QByteArray("{\"key\":\"" + key + "\",\"proof\":\"" + otherProof + "\"}")
                                  << 401;
    QTest::newRow("proof for other key") << QByteArray("{\"key\":\"" + zeroKey + "\",\"proof\":\"" + zeroKeyProof +
                                                       "\"}")
                                         << 401;
}

void TestApi::openSessionRejectsStrangers()
{
    QFETCH(QByteArray, body);
    QFETCH(int, status);

    QCOMPARE(post("/api/session", body).status, status);
}

void TestApi::confirmProvesSecret()
{
    // A low-order key with a valid proof: the server must not derive keys from an all-zero secret
    QByteArray zeroKey(32, '\0');
    QCOMPARE(hello(zeroKey, Crypto::makeHelloProof(m_secret, zeroKey)).status, 401);

    // The client finds out at once when the link does not match the server
    Crypto::KeyPair keyPair = Crypto::generateKeyPair();
    QByteArray wrongSecret = Crypto::randomBytes(Crypto::SecretSize);
    Response response = hello(keyPair.publicKey, Crypto::makeHelloProof(m_secret, keyPair.publicKey));
    QJsonObject answer = parse(response.body);
    QByteArray serverPublicKey = decode(answer.value("key"));
    QByteArray shared = Crypto::computeShared(keyPair.secretKey, serverPublicKey);
    QByteArray ad = Crypto::makeConfirmAd(decode(answer.value("session")), keyPair.publicKey, serverPublicKey);
    QByteArray plaintext;

    QVERIFY(Crypto::open(Crypto::deriveSessionKeys(m_secret, shared, keyPair.publicKey, serverPublicKey).down,
                         decode(answer.value("proof")), ad, &plaintext));
    QVERIFY(!Crypto::open(Crypto::deriveSessionKeys(wrongSecret, shared, keyPair.publicKey, serverPublicKey).down,
                          decode(answer.value("proof")), ad, &plaintext));
}

void TestApi::unknownSession_data()
{
    QTest::addColumn<QByteArray>("path");

    QTest::newRow("unknown") << QByteArray("/api/session/AAAAAAAAAAAAAAAAAAAAAA/status");
    QTest::newRow("malformed") << QByteArray("/api/session/not-a-session/status");
    QTest::newRow("empty") << QByteArray("/api/session//status");
    QTest::newRow("other api") << QByteArray("/api/other");
    QTest::newRow("session prefix") << QByteArray("/api/sessionX");
}

void TestApi::unknownSession()
{
    QFETCH(QByteArray, path);
    QCOMPARE(post(path).status, 404);
}

void TestApi::sessionsAreForgotten()
{
    QByteArray first = openSessionPath();
    QByteArray second = openSessionPath();

    for (int i = 0; i < 62; i++)
    {
        openSessionPath();
    }

    // The one used recently stays
    QCOMPARE(call(second + "/status").status, 200);

    openSessionPath();

    QCOMPARE(call(first + "/status").status, 404);
    QCOMPARE(call(second + "/status").status, 200);
}

void TestApi::onlyPost()
{
    QByteArray host = "127.0.0.1:" + QByteArray::number(m_server->serverPort());
    QCOMPARE(RawHttp::send(m_server->serverPort(), "GET", "/api/session", host).status, 405);
}

void TestApi::uploadFile()
{
    QByteArray session = openSessionPath();

    Answer response = call(session + "/file", makeAnnounce("notes.txt", 10, 1700000000000));
    QCOMPARE(response.status, 200);

    QJsonObject answer = response.object;
    QByteArray file = answer.value("file").toString().toLatin1();
    QCOMPARE(answer.value("chunk").toInteger(), ChunkSize);
    QCOMPARE(answer.value("received").toArray(), QJsonArray());

    QByteArray chunkPath = session + "/chunk/" + file + "/";

    response = call(chunkPath + "1", "4567");
    QCOMPARE(response.status, 200);
    QCOMPARE(response.object, QJsonObject({{"received", 1}, {"complete", false}}));

    response = call(chunkPath + "0", "0123");
    QCOMPARE(response.object, QJsonObject({{"received", 2}, {"complete", false}}));

    response = call(chunkPath + "2", "89");
    QCOMPARE(response.object, QJsonObject({{"received", 3}, {"complete", true}}));

    QFile result(QDir(m_uploads->dropDirectoryPath()).filePath("notes.txt"));
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), "0123456789");
}

void TestApi::resumeUpload()
{
    QByteArray session = openSessionPath();
    QByteArray file = call(session + "/file", makeAnnounce("a.bin", 10)).object.value("file").toString().toLatin1();
    QCOMPARE(call(session + "/chunk/" + file + "/0", "0123").status, 200);
    QCOMPARE(call(session + "/chunk/" + file + "/2", "89").status, 200);

    // After a page reload: a new session, the same file picked again
    QByteArray newSession = openSessionPath();
    QJsonObject answer = call(newSession + "/file", makeAnnounce("a.bin", 10)).object;

    QCOMPARE(answer.value("file").toString().toLatin1(), file);
    QCOMPARE(answer.value("received").toArray(), QJsonArray({0, 2}));

    QCOMPARE(call(newSession + "/chunk/" + file + "/1", "4567").object.value("complete").toBool(), true);
}

void TestApi::status()
{
    QByteArray session = openSessionPath();
    QCOMPARE(call(session + "/status").object, QJsonObject({{"files", QJsonArray()}}));

    QByteArray file = call(session + "/file", makeAnnounce("a.bin", 6)).object.value("file").toString().toLatin1();
    call(session + "/chunk/" + file + "/0", "0123");
    call(session + "/file", makeAnnounce("empty.txt", 0));

    QJsonArray files = call(session + "/status").object.value("files").toArray();
    QCOMPARE(files.size(), 2);

    QJsonObject first = files.at(0).toObject();
    QCOMPARE(first.value("file").toString().toLatin1(), file);
    QCOMPARE(first.value("name").toString(), "a.bin");
    QCOMPARE(first.value("size").toInteger(), 6);
    QCOMPARE(first.value("received").toInt(), 1);
    QCOMPARE(first.value("complete").toBool(), false);

    QCOMPARE(files.at(1).toObject().value("complete").toBool(), true);
}

void TestApi::announceRejectsBadRequests_data()
{
    QTest::addColumn<QByteArray>("body");

    QByteArray head = encode(QByteArray(Uploads::HeadSize, 'h')).toLatin1();

    QTest::newRow("not json") << QByteArray("hello");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("array") << QByteArray("[]");
    QTest::newRow("no name") << QByteArray("{\"size\":1,\"modified\":0,\"head\":\"" + head + "\"}");
    QTest::newRow("name not string") << QByteArray("{\"name\":1,\"size\":1,\"modified\":0,\"head\":\"" + head + "\"}");
    QTest::newRow("size string") << QByteArray("{\"name\":\"a\",\"size\":\"1\",\"modified\":0,\"head\":\"" + head + "\"}");
    QTest::newRow("size fraction") << QByteArray("{\"name\":\"a\",\"size\":1.5,\"modified\":0,\"head\":\"" + head + "\"}");
    QTest::newRow("size negative") << QByteArray("{\"name\":\"a\",\"size\":-1,\"modified\":0,\"head\":\"" + head + "\"}");
    QTest::newRow("no modified") << QByteArray("{\"name\":\"a\",\"size\":1,\"head\":\"" + head + "\"}");
    QTest::newRow("short head") << QByteArray("{\"name\":\"a\",\"size\":1,\"modified\":0,\"head\":\"AAAA\"}");
    QTest::newRow("padded head") << QByteArray("{\"name\":\"a\",\"size\":1,\"modified\":0,\"head\":\"" + head + "=\"}");
}

void TestApi::announceRejectsBadRequests()
{
    QFETCH(QByteArray, body);
    QCOMPARE(call(openSessionPath() + "/file", body).status, 400);
}

void TestApi::chunkRejectsBadRequests_data()
{
    QTest::addColumn<QByteArray>("suffix");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<int>("status");

    QTest::newRow("unknown file") << QByteArray("AAAAAAAAAAAAAAAAAAAAAA/0") << QByteArray("0123") << 404;
    QTest::newRow("malformed file") << QByteArray("xyz/0") << QByteArray("0123") << 404;
    QTest::newRow("index out of range") << QByteArray("%1/3") << QByteArray("0123") << 400;
    QTest::newRow("index negative") << QByteArray("%1/-1") << QByteArray("0123") << 400;
    QTest::newRow("index not number") << QByteArray("%1/x") << QByteArray("0123") << 400;
    QTest::newRow("index leading zero") << QByteArray("%1/01") << QByteArray("4567") << 400;
    QTest::newRow("index huge") << QByteArray("%1/99999999999") << QByteArray("0123") << 400;
    QTest::newRow("short chunk") << QByteArray("%1/0") << QByteArray("012") << 400;
    QTest::newRow("long last chunk") << QByteArray("%1/2") << QByteArray("8901") << 400;
    QTest::newRow("no index") << QByteArray("%1") << QByteArray("0123") << 404;
    QTest::newRow("extra part") << QByteArray("%1/0/x") << QByteArray("0123") << 404;
}

void TestApi::chunkRejectsBadRequests()
{
    QFETCH(QByteArray, suffix);
    QFETCH(QByteArray, body);
    QFETCH(int, status);

    QByteArray session = openSessionPath();
    QByteArray file = call(session + "/file", makeAnnounce("a.bin", 10)).object.value("file").toString().toLatin1();

    QCOMPARE(call(session + "/chunk/" + suffix.replace("%1", file), body).status, status);
    QCOMPARE(m_uploads->upload(QByteArray::fromBase64(file, QByteArray::Base64UrlEncoding)).receivedCount, 0);
}

void TestApi::bodyTooLarge()
{
    QByteArray session = openSessionPath();
    QByteArray file = call(session + "/file", makeAnnounce("a.bin", 10)).object.value("file").toString().toLatin1();

    QCOMPARE(post(session + "/chunk/" + file + "/0", QByteArray(ChunkSize + 8192, 'x')).status, 413);
}

void TestApi::rejectsTamperedRequests()
{
    Session session = openSession(m_secret);
    QByteArray path = session.path + "/file";
    QByteArray sealed = Crypto::seal(session.keys.up, makeAnnounce("a.bin", 10),
                                     Crypto::makeRequestAd(session.id, "POST", path));

    QByteArray tampered = sealed;
    tampered[tampered.size() / 2] = char(tampered[tampered.size() / 2] ^ 1);

    QCOMPARE(post(path, tampered).status, 401);
    QCOMPARE(post(path, sealed.left(Crypto::SealOverhead - 1)).status, 401);
    QCOMPARE(post(path, makeAnnounce("a.bin", 10)).status, 401); // Not sealed at all
    QVERIFY(m_uploads->uploads().isEmpty());

    QCOMPARE(post(path, sealed).status, 200);
}

void TestApi::rejectsMovedRequests()
{
    Session session = openSession(m_secret);
    QByteArray file = call(session, "file", makeAnnounce("a.bin", 10)).object.value("file").toString().toLatin1();

    QByteArray chunkPath = session.path + "/chunk/" + file + "/";
    QByteArray sealed = Crypto::seal(session.keys.up, "0123", Crypto::makeRequestAd(session.id, "POST", chunkPath + "0"));

    // A chunk sealed for one place cannot be written to another
    QCOMPARE(post(chunkPath + "1", sealed).status, 401);
    QCOMPARE(m_uploads->upload(QByteArray::fromBase64(file, QByteArray::Base64UrlEncoding)).receivedCount, 0);

    // Nor sent through another session
    Session other = openSession(m_secret);
    QCOMPARE(post(other.path + "/chunk/" + file + "/0", sealed).status, 401);

    QCOMPARE(post(chunkPath + "0", sealed).status, 200);
    QCOMPARE(m_uploads->upload(QByteArray::fromBase64(file, QByteArray::Base64UrlEncoding)).receivedCount, 1);
}

void TestApi::rejectsOtherSessionKeys()
{
    Session session = openSession(m_secret);
    Session other = openSession(m_secret);

    session.keys = other.keys;
    QCOMPARE(call(session, "status").status, 401);
}

void TestApi::answersAreSealed()
{
    Session session = openSession(m_secret);
    QByteArray path = session.path + "/status";
    QByteArray sealed = Crypto::seal(session.keys.up, QByteArray(), Crypto::makeRequestAd(session.id, "POST", path));

    Response response = post(path, sealed);
    QCOMPARE(response.status, 200);
    QCOMPARE(response.headers.value("content-type"), "application/octet-stream");
    QVERIFY(!response.body.contains("files"));

    // Bound to the request it answers
    QByteArray plaintext;
    QVERIFY(!Crypto::open(session.keys.down, response.body, Crypto::makeAnswerAd(Crypto::randomBytes(24)), &plaintext));
    QVERIFY(Crypto::open(session.keys.down, response.body, Crypto::makeAnswerAd(sealed.left(24)), &plaintext));
    QCOMPARE(parse(plaintext), QJsonObject({{"files", QJsonArray()}}));
}

QTEST_GUILESS_MAIN(TestApi)
#include "TestApi.moc"
