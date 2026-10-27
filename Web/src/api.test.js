import { describe, expect, it, vi } from 'vitest'

import { Api, ApiError } from '@/api'
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
  seal
} from '@/crypto'
import { fromBase64Url, toBase64Url } from '@/encoding'

const encode = (value) => new TextEncoder().encode(JSON.stringify(value))
const decode = (bytes) => JSON.parse(new TextDecoder().decode(bytes))

const respond = (status, body = new Uint8Array()) => ({
  ok: status >= 200 && status < 300,
  status,
  arrayBuffer: async () => body.buffer.slice(body.byteOffset, body.byteOffset + body.length)
})

// The desktop side of the protocol, in memory
class FakeServer {
  constructor(secret) {
    this.secret = secret
    this.sessions = new Map()
    this.requests = []
    this.handler = () => ({})
    this.fetch = vi.fn(async (path, options) => this.#handle(path, options.body))
  }

  forgetSessions() {
    this.sessions.clear()
  }

  #handle(path, body) {
    if (path === '/api/session') return this.#hello(decode(body))
    const [, , , sessionId, ...operation] = path.split('/')
    const session = this.sessions.get(sessionId)
    if (!session) return respond(404)
    const plaintext = open(session.keys.up, body, makeRequestAd(session.id, 'POST', path))
    if (!plaintext) return respond(401)
    this.requests.push({ operation: operation.join('/'), plaintext })
    const answer = this.handler(operation.join('/'), plaintext)
    if (typeof answer === 'number') return respond(answer)
    return respond(200, seal(session.keys.down, encode(answer), makeAnswerAd(body.subarray(0, 24))))
  }

  #hello({ key, proof }) {
    const clientPublicKey = fromBase64Url(key)
    if (toBase64Url(makeHelloProof(this.secret, clientPublicKey)) !== proof) return respond(401)
    const keyPair = makeKeyPair()
    const id = randomBytes(16)
    const keys = deriveSessionKeys(
      this.secret,
      computeShared(keyPair.secretKey, clientPublicKey),
      clientPublicKey,
      keyPair.publicKey
    )
    this.sessions.set(toBase64Url(id), { id, keys })
    const confirm = seal(
      keys.down,
      new Uint8Array(),
      makeConfirmAd(id, clientPublicKey, keyPair.publicKey)
    )
    return respond(
      200,
      encode({
        session: toBase64Url(id),
        key: toBase64Url(keyPair.publicKey),
        proof: toBase64Url(confirm)
      })
    )
  }
}

const secret = randomBytes(32)

describe('Api', () => {
  it('opens a session before the first call', async () => {
    const server = new FakeServer(secret)
    server.handler = () => ({ files: [] })

    expect(await new Api(secret, server.fetch).status()).toEqual({ files: [] })
    expect(server.fetch.mock.calls[0][0]).toBe('/api/session')
    expect(server.fetch.mock.calls[1][0]).toMatch(/^\/api\/session\/[\w-]{22}\/status$/)
    expect(server.fetch.mock.calls[1][1].method).toBe('POST')
  })

  it('announces files as sealed JSON with the head in base64url', async () => {
    const server = new FakeServer(secret)
    server.handler = () => ({ file: 'F' })

    const head = new Uint8Array([251, 255])
    expect(
      await new Api(secret, server.fetch).announce({ name: 'a.jpg', size: 5, modified: 7, head })
    ).toEqual({
      file: 'F'
    })

    const body = server.fetch.mock.calls[1][1].body
    expect(new TextDecoder().decode(body)).not.toContain('a.jpg')
    expect(server.requests[0].operation).toBe('file')
    expect(decode(server.requests[0].plaintext)).toEqual({
      name: 'a.jpg',
      size: 5,
      modified: 7,
      head: '-_8'
    })
  })

  it('sends chunks as sealed raw bytes', async () => {
    const server = new FakeServer(secret)
    server.handler = () => ({ received: 1 })
    const bytes = new Uint8Array([1, 2, 3])

    await new Api(secret, server.fetch).sendChunk('F', 4, bytes)

    expect(server.requests[0].operation).toBe('chunk/F/4')
    expect(server.requests[0].plaintext).toEqual(bytes)
    expect(server.fetch.mock.calls[1][1].body.length).toBe(3 + 40)
  })

  it('opens a new session when the server forgot the old one', async () => {
    const server = new FakeServer(secret)
    server.handler = () => ({ files: [] })
    const api = new Api(secret, server.fetch)

    await api.status()
    server.forgetSessions()
    expect(await api.status()).toEqual({ files: [] })
    expect(server.fetch.mock.calls.map(([path]) => path.split('/').length)).toEqual([3, 5, 5, 3, 5])
  })

  it('gives up when a new session does not help', async () => {
    const server = new FakeServer(secret)
    server.handler = () => 404
    await expect(new Api(secret, server.fetch).status()).rejects.toMatchObject({ status: 404 })
  })

  it('reports a link the server does not accept', async () => {
    const server = new FakeServer(secret)
    const error = await new Api(randomBytes(32), server.fetch).status().catch((error) => error)
    expect(error.status).toBe(401)
    expect(error.retryable).toBe(false)
  })

  it('refuses a server that does not know the secret', async () => {
    // Someone in the middle answers the hello with their own key and a made-up confirm
    const impostor = vi.fn(async () =>
      respond(
        200,
        encode({
          session: toBase64Url(randomBytes(16)),
          key: toBase64Url(makeKeyPair().publicKey),
          proof: toBase64Url(randomBytes(40))
        })
      )
    )
    await expect(new Api(secret, impostor).status()).rejects.toMatchObject({ status: 401 })
    expect(impostor).toHaveBeenCalledTimes(1)
  })

  it('refuses a low-order server key', async () => {
    const impostor = vi.fn(async () =>
      respond(
        200,
        encode({
          session: toBase64Url(randomBytes(16)),
          key: toBase64Url(new Uint8Array(32)),
          proof: toBase64Url(randomBytes(40))
        })
      )
    )
    await expect(new Api(secret, impostor).status()).rejects.toMatchObject({ status: 401 })
  })

  it('refuses answers that were tampered with', async () => {
    const server = new FakeServer(secret)
    server.handler = () => ({ files: [] })
    const api = new Api(secret, server.fetch)
    await api.openSession()

    const original = server.fetch.getMockImplementation()
    server.fetch.mockImplementation(async (path, options) => {
      const response = await original(path, options)
      const body = new Uint8Array(await response.arrayBuffer())
      body[body.length - 1] ^= 1
      return respond(response.status, body)
    })

    await expect(api.status()).rejects.toMatchObject({ status: 401 })
  })

  it('limits the time of every request', async () => {
    const server = new FakeServer(secret)
    await new Api(secret, server.fetch).openSession()
    expect(server.fetch.mock.calls[0][1].signal).toBeInstanceOf(AbortSignal)
  })

  it('reports lost connections as retryable', async () => {
    const fetch = vi.fn(async () => {
      throw new TypeError('Failed to fetch')
    })
    const error = await new Api(secret, fetch).status().catch((error) => error)
    expect(error).toBeInstanceOf(ApiError)
    expect(error.status).toBe(0)
    expect(error.retryable).toBe(true)
  })

  it('tells retryable errors from final ones', () => {
    expect(new ApiError(502).retryable).toBe(true)
    expect(new ApiError(500).retryable).toBe(true)
    expect(new ApiError(507).retryable).toBe(false)
    expect(new ApiError(400).retryable).toBe(false)
    expect(new ApiError(401).retryable).toBe(false)
    expect(new ApiError(413).retryable).toBe(false)
  })
})
