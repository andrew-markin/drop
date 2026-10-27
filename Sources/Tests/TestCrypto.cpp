#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "Crypto.h"

namespace {

QByteArray decode(const QJsonValue& value)
{
    return QByteArray::fromBase64(value.toString().toLatin1(), QByteArray::Base64UrlEncoding);
}

} // namespace

// Checks every step against docs/vectors/protocol.json, made by an independent implementation
class TestCrypto : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void keyPairs();
    void helloProof();
    void shared();
    void rejectsLowOrderKeys();
    void sessionKeys();
    void confirm();
    void request();
    void answer();

    void sealAndOpen();
    void openRejectsTampering();
    void randomBytes();
    void equal();

private:
    QJsonObject m_vectors;
    QJsonObject m_inputs;
};

void TestCrypto::initTestCase()
{
    QVERIFY(Crypto::init());

    QFile file(DROP_VECTORS_PATH);
    QVERIFY2(file.open(QIODevice::ReadOnly), DROP_VECTORS_PATH);
    m_vectors = QJsonDocument::fromJson(file.readAll()).object();
    m_inputs = m_vectors.value("inputs").toObject();
    QVERIFY(!m_inputs.isEmpty());
}

void TestCrypto::keyPairs()
{
    QCOMPARE(Crypto::makeKeyPair(decode(m_inputs.value("clientSecretKey"))).publicKey,
             decode(m_vectors.value("clientPublicKey")));
    QCOMPARE(Crypto::makeKeyPair(decode(m_inputs.value("serverSecretKey"))).publicKey,
             decode(m_vectors.value("serverPublicKey")));

    Crypto::KeyPair keyPair = Crypto::generateKeyPair();
    QCOMPARE(keyPair.secretKey.size(), 32);
    QCOMPARE(keyPair.publicKey, Crypto::makeKeyPair(keyPair.secretKey).publicKey);
}

void TestCrypto::helloProof()
{
    QCOMPARE(Crypto::makeHelloProof(decode(m_inputs.value("k")), decode(m_vectors.value("clientPublicKey"))),
             decode(m_vectors.value("hello")));
}

void TestCrypto::shared()
{
    QByteArray expected = decode(m_vectors.value("shared"));

    QCOMPARE(Crypto::computeShared(decode(m_inputs.value("serverSecretKey")), decode(m_vectors.value("clientPublicKey"))),
             expected);
    QCOMPARE(Crypto::computeShared(decode(m_inputs.value("clientSecretKey")), decode(m_vectors.value("serverPublicKey"))),
             expected);
}

void TestCrypto::rejectsLowOrderKeys()
{
    QByteArray secretKey = decode(m_inputs.value("serverSecretKey"));

    QVERIFY(Crypto::computeShared(secretKey, decode(m_vectors.value("lowOrderPublicKey"))).isEmpty());
    QVERIFY(Crypto::computeShared(secretKey, QByteArray(31, 'x')).isEmpty());
    QVERIFY(Crypto::computeShared(QByteArray(), decode(m_vectors.value("clientPublicKey"))).isEmpty());
}

void TestCrypto::sessionKeys()
{
    Crypto::SessionKeys keys = Crypto::deriveSessionKeys(
        decode(m_inputs.value("k")), decode(m_vectors.value("shared")), decode(m_vectors.value("clientPublicKey")),
        decode(m_vectors.value("serverPublicKey")));

    QCOMPARE(keys.up, decode(m_vectors.value("up")));
    QCOMPARE(keys.down, decode(m_vectors.value("down")));
}

void TestCrypto::confirm()
{
    QByteArray ad = Crypto::makeConfirmAd(decode(m_inputs.value("session")), decode(m_vectors.value("clientPublicKey")),
                                          decode(m_vectors.value("serverPublicKey")));
    QByteArray down = decode(m_vectors.value("down"));

    QCOMPARE(Crypto::seal(down, QByteArray(), ad, decode(m_inputs.value("confirmNonce"))),
             decode(m_vectors.value("confirm")));

    QByteArray plaintext("not empty");
    QVERIFY(Crypto::open(down, decode(m_vectors.value("confirm")), ad, &plaintext));
    QVERIFY(plaintext.isEmpty());
}

void TestCrypto::request()
{
    QJsonObject request = m_vectors.value("request").toObject();
    QByteArray ad = Crypto::makeRequestAd(decode(m_inputs.value("session")), request.value("method").toString().toLatin1(),
                                          request.value("path").toString().toLatin1());
    QByteArray up = decode(m_vectors.value("up"));
    QByteArray plaintext = request.value("plaintext").toString().toUtf8();

    QCOMPARE(Crypto::seal(up, plaintext, ad, decode(m_inputs.value("requestNonce"))), decode(request.value("sealed")));

    QByteArray opened;
    QVERIFY(Crypto::open(up, decode(request.value("sealed")), ad, &opened));
    QCOMPARE(opened, plaintext);
}

void TestCrypto::answer()
{
    QJsonObject answer = m_vectors.value("answer").toObject();
    QByteArray ad = Crypto::makeAnswerAd(decode(m_inputs.value("requestNonce")));
    QByteArray down = decode(m_vectors.value("down"));
    QByteArray plaintext = answer.value("plaintext").toString().toUtf8();

    QCOMPARE(Crypto::seal(down, plaintext, ad, decode(m_inputs.value("answerNonce"))), decode(answer.value("sealed")));
}

void TestCrypto::sealAndOpen()
{
    QByteArray key = Crypto::randomBytes(Crypto::KeySize);
    QByteArray plaintext = Crypto::randomBytes(4 * 1024 * 1024);

    QByteArray first = Crypto::seal(key, plaintext, "ad");
    QByteArray second = Crypto::seal(key, plaintext, "ad");
    QCOMPARE(first.size(), plaintext.size() + Crypto::SealOverhead);

    // A new random nonce every time
    QVERIFY(first.left(Crypto::NonceSize) != second.left(Crypto::NonceSize));

    QByteArray opened;
    QVERIFY(Crypto::open(key, first, "ad", &opened));
    QCOMPARE(opened, plaintext);
}

void TestCrypto::openRejectsTampering()
{
    QByteArray key = Crypto::randomBytes(Crypto::KeySize);
    QByteArray sealed = Crypto::seal(key, "secret", "ad");
    QByteArray plaintext = "untouched";

    QVERIFY(!Crypto::open(key, sealed, "other ad", &plaintext));
    QVERIFY(!Crypto::open(Crypto::randomBytes(Crypto::KeySize), sealed, "ad", &plaintext));
    QVERIFY(!Crypto::open(key, sealed.left(sealed.size() - 1), "ad", &plaintext));
    QVERIFY(!Crypto::open(key, sealed.left(Crypto::SealOverhead - 1), "ad", &plaintext));
    QVERIFY(!Crypto::open(QByteArray(), sealed, "ad", &plaintext));

    for (int i = 0; i < sealed.size(); i++)
    {
        QByteArray tampered = sealed;
        tampered[i] = char(tampered[i] ^ 1);
        QVERIFY(!Crypto::open(key, tampered, "ad", &plaintext));
    }

    QCOMPARE(plaintext, "untouched");
}

void TestCrypto::randomBytes()
{
    QCOMPARE(Crypto::randomBytes(32).size(), 32);
    QVERIFY(Crypto::randomBytes(32) != Crypto::randomBytes(32));
}

void TestCrypto::equal()
{
    QVERIFY(Crypto::equal("abc", "abc"));
    QVERIFY(!Crypto::equal("abc", "abd"));
    QVERIFY(!Crypto::equal("abc", "ab"));
    QVERIFY(Crypto::equal(QByteArray(), QByteArray()));
}

QTEST_GUILESS_MAIN(TestCrypto)
#include "TestCrypto.moc"
