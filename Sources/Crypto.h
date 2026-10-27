#ifndef CRYPTO_H
#define CRYPTO_H

#include <QByteArray>

// libsodium primitives and the key schedule of docs/PROTOCOL.md
namespace Crypto {

static const int SecretSize = 32;
static const int KeySize = 32;
static const int NonceSize = 24;
static const int TagSize = 16;
static const int SealOverhead = NonceSize + TagSize;

struct KeyPair
{
    QByteArray publicKey;
    QByteArray secretKey;
};

struct SessionKeys
{
    QByteArray up; // Client to server
    QByteArray down; // Server to client
};

bool init();

QByteArray randomBytes(int size);
bool equal(const QByteArray& a, const QByteArray& b);

KeyPair generateKeyPair();
KeyPair makeKeyPair(const QByteArray& secretKey);
QByteArray computeShared(const QByteArray& secretKey, const QByteArray& publicKey);

QByteArray seal(const QByteArray& key, const QByteArray& plaintext, const QByteArray& ad,
                const QByteArray& nonce = QByteArray());
bool open(const QByteArray& key, const QByteArray& sealed, const QByteArray& ad, QByteArray* plaintext);

QByteArray makeHelloProof(const QByteArray& secret, const QByteArray& clientPublicKey);
SessionKeys deriveSessionKeys(const QByteArray& secret, const QByteArray& shared, const QByteArray& clientPublicKey,
                              const QByteArray& serverPublicKey);

QByteArray makeConfirmAd(const QByteArray& sessionId, const QByteArray& clientPublicKey,
                         const QByteArray& serverPublicKey);
QByteArray makeRequestAd(const QByteArray& sessionId, const QByteArray& method, const QByteArray& path);
QByteArray makeAnswerAd(const QByteArray& requestNonce);

} // namespace Crypto

#endif // CRYPTO_H
