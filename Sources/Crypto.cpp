#include "Crypto.h"

#include <sodium.h>

namespace {

static const QByteArray HelloLabel = "Drop/1 hello";
static const QByteArray KeysLabel = "Drop/1 keys";
static const QByteArray ConfirmLabel = "Drop/1 confirm";
static const QByteArray RequestLabel = "Drop/1 request";
static const QByteArray AnswerLabel = "Drop/1 answer";

static const int HelloProofSize = 32;

const unsigned char* bytes(const QByteArray& data)
{
    return reinterpret_cast<const unsigned char*>(data.constData());
}

unsigned char* bytes(QByteArray& data)
{
    return reinterpret_cast<unsigned char*>(data.data());
}

QByteArray hash(const QByteArray& key, const QByteArray& message, int size)
{
    QByteArray result(size, Qt::Uninitialized);
    crypto_generichash(bytes(result), result.size(), bytes(message), message.size(), bytes(key), key.size());
    return result;
}

} // namespace

namespace Crypto {

bool init()
{
    return sodium_init() >= 0;
}

QByteArray randomBytes(int size)
{
    QByteArray result(size, Qt::Uninitialized);
    randombytes_buf(result.data(), result.size());
    return result;
}

bool equal(const QByteArray& a, const QByteArray& b)
{
    return a.size() == b.size() && sodium_memcmp(a.constData(), b.constData(), a.size()) == 0;
}

KeyPair generateKeyPair()
{
    return makeKeyPair(randomBytes(crypto_scalarmult_SCALARBYTES));
}

KeyPair makeKeyPair(const QByteArray& secretKey)
{
    KeyPair keyPair;
    keyPair.secretKey = secretKey;
    keyPair.publicKey.resize(crypto_scalarmult_BYTES);
    crypto_scalarmult_base(bytes(keyPair.publicKey), bytes(secretKey));
    return keyPair;
}

QByteArray computeShared(const QByteArray& secretKey, const QByteArray& publicKey)
{
    if (secretKey.size() != crypto_scalarmult_SCALARBYTES || publicKey.size() != crypto_scalarmult_BYTES)
    {
        return QByteArray();
    }

    QByteArray shared(crypto_scalarmult_BYTES, Qt::Uninitialized);

    // Fails for low-order public keys, whose result is all zeros
    if (crypto_scalarmult(bytes(shared), bytes(secretKey), bytes(publicKey)) != 0)
    {
        return QByteArray();
    }

    return shared;
}

QByteArray seal(const QByteArray& key, const QByteArray& plaintext, const QByteArray& ad, const QByteArray& nonce)
{
    QByteArray result = nonce.isEmpty() ? randomBytes(NonceSize) : nonce;
    result.resize(NonceSize + plaintext.size() + TagSize);

    unsigned long long cipherSize = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(bytes(result) + NonceSize, &cipherSize, bytes(plaintext),
                                               plaintext.size(), bytes(ad), ad.size(), nullptr, bytes(result),
                                               bytes(key));
    return result;
}

bool open(const QByteArray& key, const QByteArray& sealed, const QByteArray& ad, QByteArray* plaintext)
{
    if (key.size() != KeySize || sealed.size() < SealOverhead)
    {
        return false;
    }

    QByteArray result(sealed.size() - SealOverhead, Qt::Uninitialized);
    unsigned long long resultSize = 0;

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(bytes(result), &resultSize, nullptr, bytes(sealed) + NonceSize,
                                                   sealed.size() - NonceSize, bytes(ad), ad.size(), bytes(sealed),
                                                   bytes(key)) != 0)
    {
        return false;
    }

    *plaintext = result;
    return true;
}

QByteArray makeHelloProof(const QByteArray& secret, const QByteArray& clientPublicKey)
{
    return hash(secret, HelloLabel + clientPublicKey, HelloProofSize);
}

SessionKeys deriveSessionKeys(const QByteArray& secret, const QByteArray& shared, const QByteArray& clientPublicKey,
                              const QByteArray& serverPublicKey)
{
    QByteArray keys = hash(secret, KeysLabel + shared + clientPublicKey + serverPublicKey, 2 * KeySize);
    return SessionKeys{keys.left(KeySize), keys.mid(KeySize)};
}

QByteArray makeConfirmAd(const QByteArray& sessionId, const QByteArray& clientPublicKey,
                         const QByteArray& serverPublicKey)
{
    return ConfirmLabel + sessionId + clientPublicKey + serverPublicKey;
}

QByteArray makeRequestAd(const QByteArray& sessionId, const QByteArray& method, const QByteArray& path)
{
    return RequestLabel + sessionId + method + ' ' + path;
}

QByteArray makeAnswerAd(const QByteArray& requestNonce)
{
    return AnswerLabel + requestNonce;
}

} // namespace Crypto
