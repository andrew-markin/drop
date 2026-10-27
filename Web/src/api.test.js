import { describe, expect, it, vi } from 'vitest'

import { Api, ApiError } from '@/api'

const respond = (status, body = {}) => ({
  ok: status >= 200 && status < 300,
  status,
  json: async () => body
})

function makeFetch(handler) {
  return vi.fn(async (path, options) => handler(path, options))
}

describe('Api', () => {
  it('opens a session before the first call', async () => {
    const fetch = makeFetch((path) =>
      path === '/api/session' ? respond(200, { session: 'S1' }) : respond(200, { files: [] })
    )
    const api = new Api(fetch)

    expect(await api.status()).toEqual({ files: [] })
    expect(fetch.mock.calls.map(([path]) => path)).toEqual([
      '/api/session',
      '/api/session/S1/status'
    ])
    expect(fetch.mock.calls[1][1].method).toBe('POST')
  })

  it('announces files as JSON with the head in base64url', async () => {
    const fetch = makeFetch((path) =>
      path === '/api/session' ? respond(200, { session: 'S1' }) : respond(200, { file: 'F' })
    )
    const api = new Api(fetch)

    await api.announce({ name: 'a.jpg', size: 5, modified: 7, head: new Uint8Array([251, 255]) })

    const [path, { body }] = fetch.mock.calls[1]
    expect(path).toBe('/api/session/S1/file')
    expect(JSON.parse(new TextDecoder().decode(body))).toEqual({
      name: 'a.jpg',
      size: 5,
      modified: 7,
      head: '-_8'
    })
  })

  it('sends chunks as raw bytes', async () => {
    const fetch = makeFetch((path) =>
      path === '/api/session' ? respond(200, { session: 'S1' }) : respond(200, { received: 1 })
    )
    const bytes = new Uint8Array([1, 2, 3])

    await new Api(fetch).sendChunk('F', 4, bytes)

    expect(fetch.mock.calls[1][0]).toBe('/api/session/S1/chunk/F/4')
    expect(fetch.mock.calls[1][1].body).toBe(bytes)
  })

  it('opens a new session when the server forgot the old one', async () => {
    let sessions = 0
    const fetch = makeFetch((path) => {
      if (path === '/api/session') return respond(200, { session: `S${++sessions}` })
      return path.startsWith('/api/session/S1/') ? respond(404) : respond(200, { files: [] })
    })

    expect(await new Api(fetch).status()).toEqual({ files: [] })
    expect(fetch.mock.calls.map(([path]) => path)).toEqual([
      '/api/session',
      '/api/session/S1/status',
      '/api/session',
      '/api/session/S2/status'
    ])
  })

  it('gives up when a new session does not help', async () => {
    const fetch = makeFetch((path) =>
      path === '/api/session' ? respond(200, { session: 'S' }) : respond(404)
    )
    await expect(new Api(fetch).status()).rejects.toMatchObject({ status: 404 })
  })

  it('reports lost connections as retryable', async () => {
    const fetch = vi.fn(async () => {
      throw new TypeError('Failed to fetch')
    })
    const error = await new Api(fetch).status().catch((error) => error)
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
