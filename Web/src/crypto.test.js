import { describe, expect, it } from 'vitest'

import {
  computeShared,
  deriveSessionKeys,
  makeAnswerAd,
  makeConfirmAd,
  makeHelloProof,
  makeKeyPair,
  makeRequestAd,
  open,
  randomBytes,
  seal,
  SealOverhead
} from '@/crypto'
import { fromBase64Url } from '@/encoding'

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
