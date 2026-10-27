import { toBase64Url } from '@/encoding'

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
  #fetch
  #session = undefined

  constructor(fetchImpl = (...args) => fetch(...args)) {
    this.#fetch = fetchImpl
  }

  async openSession() {
    const answer = await this.#post('/api/session')
    this.#session = answer.session
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
  async #call(operation, body) {
    if (!this.#session) await this.openSession()
    try {
      return await this.#post(`/api/session/${this.#session}/${operation}`, body)
    } catch (error) {
      if (!(error instanceof ApiError) || error.status !== 404) throw error
      await this.openSession()
      return await this.#post(`/api/session/${this.#session}/${operation}`, body)
    }
  }

  async #post(path, body = new Uint8Array()) {
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
    return response.json()
  }
}
