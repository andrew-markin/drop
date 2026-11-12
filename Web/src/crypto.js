import { xchacha20poly1305 } from '@noble/ciphers/chacha.js'
import { concatBytes, utf8ToBytes } from '@noble/ciphers/utils.js'
import { x25519 } from '@noble/curves/ed25519.js'
import { blake2b } from '@noble/hashes/blake2.js'

import { fromBase64Url } from '@/utils'

// The key schedule and sealing of docs/PROTOCOL.md. crypto.subtle is not available over plain
// HTTP, so everything is done by @noble packages; crypto.getRandomValues works anywhere.

export const SecretLength = 32
export const KeySize = 32
export const NonceSize = 24
export const TagSize = 16
export const SealOverhead = NonceSize + TagSize

const HelloLabel = utf8ToBytes('Drop/1 hello')
const KeysLabel = utf8ToBytes('Drop/1 keys')
const ConfirmLabel = utf8ToBytes('Drop/1 confirm')
const RequestLabel = utf8ToBytes('Drop/1 request')
const AnswerLabel = utf8ToBytes('Drop/1 answer')

// Matches a resumed upload to the same file, see "Announce a file" in docs/PROTOCOL.md
export const HeadSampleSize = 1024 * 1024
export const HeadSize = 32

export const randomBytes = (size) => crypto.getRandomValues(new Uint8Array(size))

// The link secret comes in the URL fragment, which the browser never sends over the network:
// http://192.168.0.5:54246/#k=<base64url(k)>
export function readSecret(hash) {
  const params = new URLSearchParams(hash.replace(/^#/, ''))
  const secret = fromBase64Url(params.get('k') ?? '')
  return secret?.length === SecretLength ? secret : undefined
}

export function makeKeyPair(secretKey = randomBytes(KeySize)) {
  return { secretKey, publicKey: x25519.getPublicKey(secretKey) }
}

// Undefined for a low-order public key, whose result would be all zeros
export function computeShared(secretKey, publicKey) {
  try {
    return x25519.getSharedSecret(secretKey, publicKey)
  } catch {
    return undefined
  }
}

export function makeHelloProof(secret, clientPublicKey) {
  return blake2b(concatBytes(HelloLabel, clientPublicKey), { key: secret, dkLen: 32 })
}

export function deriveSessionKeys(secret, shared, clientPublicKey, serverPublicKey) {
  const message = concatBytes(KeysLabel, shared, clientPublicKey, serverPublicKey)
  const keys = blake2b(message, { key: secret, dkLen: 2 * KeySize })
  return { up: keys.slice(0, KeySize), down: keys.slice(KeySize) }
}

export const makeConfirmAd = (session, clientPublicKey, serverPublicKey) =>
  concatBytes(ConfirmLabel, session, clientPublicKey, serverPublicKey)

export const makeRequestAd = (session, method, path) =>
  concatBytes(RequestLabel, session, utf8ToBytes(`${method} ${path}`))

export const makeAnswerAd = (requestNonce) => concatBytes(AnswerLabel, requestNonce)

export function seal(key, plaintext, ad, nonce = randomBytes(NonceSize)) {
  return concatBytes(nonce, xchacha20poly1305(key, nonce, ad).encrypt(plaintext))
}

// Undefined for anything that was not sealed with this key and associated data
export function open(key, sealed, ad) {
  if (sealed.length < SealOverhead) return undefined
  try {
    return xchacha20poly1305(key, sealed.subarray(0, NonceSize), ad).decrypt(
      sealed.subarray(NonceSize)
    )
  } catch {
    return undefined
  }
}

export async function computeHead(file) {
  const sample = new Uint8Array(await file.slice(0, HeadSampleSize).arrayBuffer())
  return blake2b(sample, { dkLen: HeadSize })
}
