import {
  computeShared,
  deriveSessionKeys,
  makeAnswerAd,
  makeConfirmAd,
  makeHelloProof,
  makeKeyPair,
  makeRequestAd,
  NonceSize,
  open,
  seal
} from '@/crypto'
import { fromBase64Url, toBase64Url } from '@/encoding'

// A request hangs rather than fails when Wi-Fi drops, a chunk takes seconds even on a slow network
export const RequestTimeout = 60000 // 1m

// Answers that mean a request went wrong for good, see "Errors" in docs/PROTOCOL.md
export class ApiError extends Error {
  constructor(status) {
    super(`Server answered ${status}`)
    this.name = 'ApiError'
    this.status = status
  }

  // Worth trying again later: the network or the server had a hiccup
  get retryable() {
    return this.status === 0 || (this.status >= 500 && this.status !== 507)
  }
}

export class Api {
  #secret
  #fetch
  #session = undefined

  constructor(secret, fetchImpl = (...args) => fetch(...args)) {
    this.#secret = secret
    this.#fetch = fetchImpl
  }

  // The hello: a one-time key exchange mixed with the link secret, see "Session" in the protocol
  async openSession() {
    const keyPair = makeKeyPair()
    const hello = {
      key: toBase64Url(keyPair.publicKey),
      proof: toBase64Url(makeHelloProof(this.#secret, keyPair.publicKey))
    }
    const response = await this.#post(
      '/api/session',
      new TextEncoder().encode(JSON.stringify(hello))
    )
    const answer = JSON.parse(new TextDecoder().decode(response))

    const id = fromBase64Url(answer.session)
    const serverPublicKey = fromBase64Url(answer.key)
    const confirm = fromBase64Url(answer.proof)
    const shared = serverPublicKey && computeShared(keyPair.secretKey, serverPublicKey)

    // Whoever answered does not know the secret: a wrong link or someone in the middle
    if (!id || !shared || !confirm) throw new ApiError(401)
    const keys = deriveSessionKeys(this.#secret, shared, keyPair.publicKey, serverPublicKey)
    const confirmAd = makeConfirmAd(id, keyPair.publicKey, serverPublicKey)
    if (!open(keys.down, confirm, confirmAd)) throw new ApiError(401)

    this.#session = { id, path: `/api/session/${answer.session}`, keys }
  }

  announce({ name, size, modified, head }) {
    const body = JSON.stringify({ name, size, modified, head: toBase64Url(head) })
    return this.#call('file', new TextEncoder().encode(body))
  }

  sendChunk(file, index, bytes) {
    return this.#call(`chunk/${file}/${index}`, bytes)
  }

  status() {
    return this.#call('status', new Uint8Array())
  }

  // The server forgets sessions of pages it has not heard from for a while,
  // the uploads themselves outlive sessions, so a new one picks up where the old one stopped
  async #call(operation, plaintext) {
    if (!this.#session) await this.openSession()
    try {
      return await this.#sealedPost(operation, plaintext)
    } catch (error) {
      if (!(error instanceof ApiError) || error.status !== 404) throw error
      await this.openSession()
      return await this.#sealedPost(operation, plaintext)
    }
  }

  async #sealedPost(operation, plaintext) {
    const { id, path: sessionPath, keys } = this.#session
    const path = `${sessionPath}/${operation}`
    const sealed = seal(keys.up, plaintext, makeRequestAd(id, 'POST', path))
    const answer = open(
      keys.down,
      await this.#post(path, sealed),
      makeAnswerAd(sealed.subarray(0, NonceSize))
    )
    if (!answer) throw new ApiError(401) // Not from the server that knows the session keys
    return JSON.parse(new TextDecoder().decode(answer))
  }

  async #post(path, body) {
    let response
    try {
      response = await this.#fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/octet-stream' },
        body,
        cache: 'no-store',
        signal: AbortSignal.timeout(RequestTimeout)
      })
    } catch {
      throw new ApiError(0) // No connection, or no answer in time
    }
    if (!response.ok) throw new ApiError(response.status)
    try {
      return new Uint8Array(await response.arrayBuffer())
    } catch {
      throw new ApiError(0) // The connection broke in the middle of the answer
    }
  }
}
