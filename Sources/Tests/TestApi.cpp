#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

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

QJsonObject parse(const Response& response)
{
    return QJsonDocument::fromJson(response.body).object();
}

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

private:
    Response post(const QByteArray& path, const QByteArray& body = QByteArray());
    QByteArray openSessionPath();

private:
    QTemporaryDir* m_temporaryDir = nullptr;
    Uploads* m_uploads = nullptr;
    WebServer* m_server = nullptr;
};

void TestApi::init()
{
    m_temporaryDir = new QTemporaryDir();
    QVERIFY(m_temporaryDir->isValid());

    m_uploads = new Uploads(QDir(m_temporaryDir->path()), ChunkSize);
    m_server = new WebServer(":/Web", m_uploads);
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

QByteArray TestApi::openSessionPath()
{
    Response response = post("/api/session");
    return "/api/session/" + parse(response).value("session").toString().toLatin1();
}

void TestApi::openSession()
{
    Response response = post("/api/session");
    QCOMPARE(response.status, 200);
    QCOMPARE(response.headers.value("cache-control"), "no-store");

    QString session = parse(response).value("session").toString();
    QCOMPARE(QByteArray::fromBase64(session.toLatin1(), QByteArray::Base64UrlEncoding).size(), 16);

    // Every page gets a session of its own
    QVERIFY(parse(post("/api/session")).value("session").toString() != session);

    QCOMPARE(post("/api/session/" + session.toLatin1() + "/status").status, 200);
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
    QCOMPARE(post(second + "/status").status, 200);

    openSessionPath();

    QCOMPARE(post(first + "/status").status, 404);
    QCOMPARE(post(second + "/status").status, 200);
}

void TestApi::onlyPost()
{
    QByteArray host = "127.0.0.1:" + QByteArray::number(m_server->serverPort());
    QCOMPARE(RawHttp::send(m_server->serverPort(), "GET", "/api/session", host).status, 405);
}

void TestApi::uploadFile()
{
    QByteArray session = openSessionPath();

    Response response = post(session + "/file", makeAnnounce("notes.txt", 10, 1700000000000));
    QCOMPARE(response.status, 200);

    QJsonObject answer = parse(response);
    QByteArray file = answer.value("file").toString().toLatin1();
    QCOMPARE(answer.value("chunk").toInteger(), ChunkSize);
    QCOMPARE(answer.value("received").toArray(), QJsonArray());

    QByteArray chunkPath = session + "/chunk/" + file + "/";

    response = post(chunkPath + "1", "4567");
    QCOMPARE(response.status, 200);
    QCOMPARE(parse(response), QJsonObject({{"received", 1}, {"complete", false}}));

    response = post(chunkPath + "0", "0123");
    QCOMPARE(parse(response), QJsonObject({{"received", 2}, {"complete", false}}));

    response = post(chunkPath + "2", "89");
    QCOMPARE(parse(response), QJsonObject({{"received", 3}, {"complete", true}}));

    QFile result(QDir(m_uploads->dropDirectoryPath()).filePath("notes.txt"));
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), "0123456789");
}

void TestApi::resumeUpload()
{
    QByteArray session = openSessionPath();
    QByteArray file = parse(post(session + "/file", makeAnnounce("a.bin", 10))).value("file").toString().toLatin1();
    QCOMPARE(post(session + "/chunk/" + file + "/0", "0123").status, 200);
    QCOMPARE(post(session + "/chunk/" + file + "/2", "89").status, 200);

    // After a page reload: a new session, the same file picked again
    QByteArray newSession = openSessionPath();
    QJsonObject answer = parse(post(newSession + "/file", makeAnnounce("a.bin", 10)));

    QCOMPARE(answer.value("file").toString().toLatin1(), file);
    QCOMPARE(answer.value("received").toArray(), QJsonArray({0, 2}));

    QCOMPARE(parse(post(newSession + "/chunk/" + file + "/1", "4567")).value("complete").toBool(), true);
}

void TestApi::status()
{
    QByteArray session = openSessionPath();
    QCOMPARE(parse(post(session + "/status")), QJsonObject({{"files", QJsonArray()}}));

    QByteArray file = parse(post(session + "/file", makeAnnounce("a.bin", 6))).value("file").toString().toLatin1();
    post(session + "/chunk/" + file + "/0", "0123");
    post(session + "/file", makeAnnounce("empty.txt", 0));

    QJsonArray files = parse(post(session + "/status")).value("files").toArray();
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
    QCOMPARE(post(openSessionPath() + "/file", body).status, 400);
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
    QByteArray file = parse(post(session + "/file", makeAnnounce("a.bin", 10))).value("file").toString().toLatin1();

    QCOMPARE(post(session + "/chunk/" + suffix.replace("%1", file), body).status, status);
    QCOMPARE(m_uploads->upload(QByteArray::fromBase64(file, QByteArray::Base64UrlEncoding)).receivedCount, 0);
}

void TestApi::bodyTooLarge()
{
    QByteArray session = openSessionPath();
    QByteArray file = parse(post(session + "/file", makeAnnounce("a.bin", 10))).value("file").toString().toLatin1();

    QCOMPARE(post(session + "/chunk/" + file + "/0", QByteArray(ChunkSize + 8192, 'x')).status, 413);
}

QTEST_GUILESS_MAIN(TestApi)
#include "TestApi.moc"
