#include <QFile>
#include <QTest>

#include "RawHttp.h"
#include "WebServer.h"

using RawHttp::Response;

class TestWebServer : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void listensOnRandomPort();

    void servesIndex();
    void servesAssets();
    void setsSecurityHeaders();

    void rejectsMissingFiles_data();
    void rejectsMissingFiles();

    void rejectsForeignHost_data();
    void rejectsForeignHost();

    void rejectsOtherMethods();

private:
    Response send(const QByteArray& method, const QByteArray& path, const QByteArray& host = QByteArray());
    QByteArray getHost() const;

private:
    WebServer* m_server = nullptr;
};

void TestWebServer::init()
{
    m_server = new WebServer(":/Web", nullptr, QByteArray(), this);
    QVERIFY2(m_server->start(QHostAddress::LocalHost), qPrintable(m_server->errorString()));
}

void TestWebServer::cleanup()
{
    delete m_server;
    m_server = nullptr;
}

QByteArray TestWebServer::getHost() const
{
    return "127.0.0.1:" + QByteArray::number(m_server->serverPort());
}

Response TestWebServer::send(const QByteArray& method, const QByteArray& path, const QByteArray& host)
{
    return RawHttp::send(m_server->serverPort(), method, path, host.isNull() ? getHost() : host);
}

void TestWebServer::listensOnRandomPort()
{
    WebServer other(":/Web", nullptr, QByteArray());
    QVERIFY(other.start(QHostAddress::LocalHost));

    QVERIFY(m_server->serverPort() != 0);
    QVERIFY(other.serverPort() != 0);
    QVERIFY(other.serverPort() != m_server->serverPort());
    QCOMPARE(m_server->serverAddress(), QHostAddress(QHostAddress::LocalHost));
}

void TestWebServer::servesIndex()
{
    QFile index(":/Web/index.html");
    QVERIFY(index.open(QIODevice::ReadOnly));
    QByteArray expected = index.readAll();

    for (const QByteArray& path : {QByteArray("/"), QByteArray("/index.html")})
    {
        Response response = send("GET", path);
        QCOMPARE(response.status, 200);
        QCOMPARE(response.body, expected);
        QCOMPARE(response.headers.value("content-type"), "text/html; charset=utf-8");
        QCOMPARE(response.headers.value("cache-control"), "no-store");
    }

    // The fragment never reaches the server, but a query might, and changes nothing
    QCOMPARE(send("GET", "/?utm=1").body, expected);
}

void TestWebServer::servesAssets()
{
    Response response = send("GET", "/assets/app-0123abcd.js");
    QCOMPARE(response.status, 200);
    QCOMPARE(response.body, "console.log(\"app\")\n");
    QCOMPARE(response.headers.value("content-type"), "text/javascript; charset=utf-8");
    QCOMPARE(response.headers.value("cache-control"), "public, max-age=31536000, immutable");

    response = send("GET", "/favicon.svg");
    QCOMPARE(response.status, 200);
    QCOMPARE(response.headers.value("content-type"), "image/svg+xml");
    QCOMPARE(response.headers.value("cache-control"), "no-store");
}

void TestWebServer::setsSecurityHeaders()
{
    for (const QByteArray& path : {QByteArray("/"), QByteArray("/missing")})
    {
        Response response = send("GET", path);
        QVERIFY(response.headers.value("content-security-policy").startsWith("default-src 'self';"));
        QCOMPARE(response.headers.value("x-content-type-options"), "nosniff");
        QCOMPARE(response.headers.value("x-frame-options"), "DENY");
        QCOMPARE(response.headers.value("referrer-policy"), "no-referrer");
    }
}

void TestWebServer::rejectsMissingFiles_data()
{
    QTest::addColumn<QByteArray>("path");

    QTest::newRow("missing") << QByteArray("/missing.js");
    QTest::newRow("directory") << QByteArray("/assets");
    QTest::newRow("directory slash") << QByteArray("/assets/");
    QTest::newRow("parent") << QByteArray("/../Web/index.html");
    QTest::newRow("encoded parent") << QByteArray("/%2e%2e/Web/index.html");
    QTest::newRow("inner parent") << QByteArray("/assets/../index.html");
    QTest::newRow("double slash") << QByteArray("//index.html");
    QTest::newRow("resource outside") << QByteArray("/../qt-project.org/qmime/freedesktop.org.xml");
}

void TestWebServer::rejectsMissingFiles()
{
    QFETCH(QByteArray, path);

    Response response = send("GET", path);
    QCOMPARE(response.status, 404);
    QVERIFY(response.body.isEmpty());
}

void TestWebServer::rejectsForeignHost_data()
{
    QTest::addColumn<QByteArray>("host");

    QTest::newRow("rebinding") << QByteArray("attacker.example");
    QTest::newRow("rebinding with port") << ("attacker.example:" + QByteArray::number(54246));
    QTest::newRow("localhost name") << QByteArray("localhost");
    QTest::newRow("no port") << QByteArray("127.0.0.1");
    QTest::newRow("empty") << QByteArray("");
}

void TestWebServer::rejectsForeignHost()
{
    QFETCH(QByteArray, host);

    Response response = send("GET", "/", host);
    QCOMPARE(response.status, 421);
    QVERIFY(response.body.isEmpty());
}

void TestWebServer::rejectsOtherMethods()
{
    QCOMPARE(send("POST", "/").status, 405);
    QCOMPARE(send("PUT", "/index.html").status, 405);
    QCOMPARE(send("DELETE", "/index.html").status, 405);
}

QTEST_GUILESS_MAIN(TestWebServer)
#include "TestWebServer.moc"
