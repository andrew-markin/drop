import { blake2b } from '@noble/hashes/blake2.js'
import { describe, expect, it } from 'vitest'

import {
  computeHead,
  computeShared,
  deriveSessionKeys,
  HeadSampleSize,
  makeAnswerAd,
  makeConfirmAd,
  makeHelloProof,
  makeKeyPair,
  makeRequestAd,
  open,
  randomBytes,
  readSecret,
  seal,
  SealOverhead,
  SecretLength
} from '@/crypto'
import { fromBase64Url, toBase64Url } from '@/utils'

import vectors from '../../docs/vectors/protocol.json'

// Every step against docs/vectors/protocol.json, made by an independent implementation
const inputs = Object.fromEntries(
  Object.entries(vectors.inputs).map(([name, value]) => [name, fromBase64Url(value)])
)
const v = (name) => fromBase64Url(vectors[name])
const text = (value) => new TextEncoder().encode(value)

describe('protocol vectors', () => {
  it('makes key pairs', () => {
    expect(makeKeyPair(inputs.clientSecretKey).publicKey).toEqual(v('clientPublicKey'))
    expect(makeKeyPair(inputs.serverSecretKey).publicKey).toEqual(v('serverPublicKey'))
  })

  it('proves the secret in the hello', () => {
    expect(makeHelloProof(inputs.k, v('clientPublicKey'))).toEqual(v('hello'))
  })

  it('computes the shared secret on both sides', () => {
    expect(computeShared(inputs.clientSecretKey, v('serverPublicKey'))).toEqual(v('shared'))
    expect(computeShared(inputs.serverSecretKey, v('clientPublicKey'))).toEqual(v('shared'))
  })

  it('refuses low-order public keys', () => {
    expect(computeShared(inputs.clientSecretKey, v('lowOrderPublicKey'))).toBeUndefined()
  })

  it('derives the session keys', () => {
    const keys = deriveSessionKeys(
      inputs.k,
      v('shared'),
      v('clientPublicKey'),
      v('serverPublicKey')
    )
    expect(keys.up).toEqual(v('up'))
    expect(keys.down).toEqual(v('down'))
  })

  it('opens the confirm', () => {
    const ad = makeConfirmAd(inputs.session, v('clientPublicKey'), v('serverPublicKey'))
    expect(seal(v('down'), new Uint8Array(), ad, inputs.confirmNonce)).toEqual(v('confirm'))
    expect(open(v('down'), v('confirm'), ad)).toEqual(new Uint8Array())
    expect(open(v('up'), v('confirm'), ad)).toBeUndefined()
  })

  it('seals the request', () => {
    const { method, path, plaintext, sealed } = vectors.request
    const ad = makeRequestAd(inputs.session, method, path)
    expect(seal(v('up'), text(plaintext), ad, inputs.requestNonce)).toEqual(fromBase64Url(sealed))
  })

  it('opens the answer', () => {
    const { plaintext, sealed } = vectors.answer
    const ad = makeAnswerAd(inputs.requestNonce)
    expect(open(v('down'), fromBase64Url(sealed), ad)).toEqual(text(plaintext))
  })
})

describe('seal and open', () => {
  const key = randomBytes(32)

  it('uses a new nonce every time', () => {
    const first = seal(key, text('secret'), text('ad'))
    const second = seal(key, text('secret'), text('ad'))
    expect(first.length).toBe(6 + SealOverhead)
    expect(first.subarray(0, 24)).not.toEqual(second.subarray(0, 24))
    expect(open(key, first, text('ad'))).toEqual(text('secret'))
  })

  it('refuses anything tampered with', () => {
    const sealed = seal(key, text('secret'), text('ad'))
    expect(open(key, sealed, text('other'))).toBeUndefined()
    expect(open(randomBytes(32), sealed, text('ad'))).toBeUndefined()
    expect(open(key, sealed.subarray(0, SealOverhead - 1), text('ad'))).toBeUndefined()
    for (let index = 0; index < sealed.length; index++) {
      const tampered = sealed.slice()
      tampered[index] ^= 1
      expect(open(key, tampered, text('ad'))).toBeUndefined()
    }
  })
})

describe('readSecret', () => {
  const secret = Uint8Array.from({ length: SecretLength }, (_, index) => index * 7)
  const encoded = toBase64Url(secret)

  it('reads the secret from the fragment', () => {
    expect(readSecret(`#k=${encoded}`)).toEqual(secret)
    expect(readSecret(`k=${encoded}`)).toEqual(secret)
  })

  it('ignores other parameters', () => {
    expect(readSecret(`#x=1&k=${encoded}&y=2`)).toEqual(secret)
  })

  it('rejects a missing, malformed or wrong length secret', () => {
    expect(readSecret('')).toBeUndefined()
    expect(readSecret('#')).toBeUndefined()
    expect(readSecret('#k=')).toBeUndefined()
    expect(readSecret(`#k=${encoded}=`)).toBeUndefined()
    expect(readSecret(`#k=${encoded.slice(0, -2)}`)).toBeUndefined()
    expect(readSecret(`#k=${toBase64Url(new Uint8Array(SecretLength + 1))}`)).toBeUndefined()
  })
})

describe('computeHead', () => {
  it('hashes nothing for an empty file', async () => {
    // BLAKE2b-256 of the empty string, the same value libsodium gives
    const head = await computeHead(new Blob([]))
    expect(toBase64Url(head)).toBe('DldRwCblQ7Loqy6wYJnaodHl30d3j3eH-qtFzfEv46g')
  })

  it('hashes a short file whole', async () => {
    const bytes = new TextEncoder().encode('hello')
    expect(await computeHead(new Blob([bytes]))).toEqual(blake2b(bytes, { dkLen: 32 }))
  })

  it('hashes only the first MiB of a long file', async () => {
    const bytes = Uint8Array.from({ length: HeadSampleSize + 10 }, (_, index) => index % 251)
    const head = await computeHead(new Blob([bytes]))
    expect(head).toEqual(blake2b(bytes.subarray(0, HeadSampleSize), { dkLen: 32 }))

    // Changes after the first MiB do not count
    bytes[HeadSampleSize + 5] ^= 1
    expect(await computeHead(new Blob([bytes]))).toEqual(head)
  })
})
