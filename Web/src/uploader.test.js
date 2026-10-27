import { describe, expect, it } from 'vitest'

import { ApiError } from '@/api'
import { ChunksInFlight, RetryDelayMax, RetryDelayMin, Uploader } from '@/uploader'

// Keeps files in memory the way the desktop application keeps them on disk
class FakeServer {
  constructor({ chunk = 4 } = {}) {
    this.chunk = chunk
    this.files = new Map()
    this.failures = [] // Errors to throw on the next calls, in order
    this.inFlight = 0
    this.maxInFlight = 0
  }

  async announce({ name, size, modified, head }) {
    this.#maybeFail()
    const key = `${name}/${size}/${modified}/${head.join(',')}`
    if (!this.files.has(key)) {
      this.files.set(key, { name, size, chunks: new Map() })
    }
    return { file: key, chunk: this.chunk, received: [...this.files.get(key).chunks.keys()] }
  }

  async sendChunk(file, index, bytes) {
    this.inFlight++
    this.maxInFlight = Math.max(this.maxInFlight, this.inFlight)
    await Promise.resolve()
    this.inFlight--
    this.#maybeFail()
    this.files.get(file).chunks.set(index, bytes)
    return { received: this.files.get(file).chunks.size }
  }

  content(name) {
    const file = [...this.files.values()].find((file) => file.name === name)
    const indexes = [...file.chunks.keys()].sort((a, b) => a - b)
    return new TextDecoder().decode(
      new Uint8Array(indexes.flatMap((index) => [...file.chunks.get(index)]))
    )
  }

  #maybeFail() {
    const failure = this.failures.shift()
    if (failure) throw failure
  }
}

const makeFile = (name, text, lastModified = 1000) => new File([text], name, { lastModified })

function makeUploader(server) {
  const delays = []
  const uploader = new Uploader(server, { wait: async (delay) => delays.push(delay) })
  return { uploader, delays }
}

describe('Uploader', () => {
  it('sends files in chunks', async () => {
    const server = new FakeServer()
    const { uploader } = makeUploader(server)

    await uploader.add([makeFile('a.txt', '0123456789'), makeFile('b.txt', 'xy')])

    expect(server.content('a.txt')).toBe('0123456789')
    expect(server.content('b.txt')).toBe('xy')
    expect(uploader.items.map((item) => item.status)).toEqual(['done', 'done'])
    expect(uploader.sentSize).toBe(12)
    expect(uploader.totalSize).toBe(12)
    expect(uploader.busy).toBe(false)
  })

  it('keeps a few chunks in flight', async () => {
    const server = new FakeServer({ chunk: 1 })
    const { uploader } = makeUploader(server)

    await uploader.add([makeFile('a.txt', 'abcdefghij')])

    expect(server.content('a.txt')).toBe('abcdefghij')
    expect(server.maxInFlight).toBe(ChunksInFlight)
  })

  it('handles an empty file', async () => {
    const server = new FakeServer()
    const { uploader } = makeUploader(server)

    await uploader.add([makeFile('empty.txt', '')])

    expect(uploader.items[0].status).toBe('done')
    expect(server.files.size).toBe(1)
  })

  it('skips chunks the server already has', async () => {
    const server = new FakeServer()
    const { uploader } = makeUploader(server)
    const file = makeFile('a.txt', '0123456789')

    // As if the page was reloaded in the middle of the upload
    server.failures = [undefined, undefined, new ApiError(400)]
    await uploader.add([file])
    expect(uploader.items[0].status).toBe('failed')

    const sentBefore = [...server.files.values()][0].chunks.size
    expect(sentBefore).toBeGreaterThan(0)

    const { uploader: resumed } = makeUploader(server)
    let sentAtStart
    const original = server.sendChunk.bind(server)
    server.sendChunk = (file, index, bytes) => {
      sentAtStart ??= resumed.items[0].sent
      return original(file, index, bytes)
    }

    await resumed.add([makeFile('a.txt', '0123456789')])
    expect(resumed.items[0].status).toBe('done')
    expect(sentAtStart).toBeGreaterThan(0)
    expect(server.content('a.txt')).toBe('0123456789')
  })

  it('retries after lost connections with a growing delay', async () => {
    const server = new FakeServer()
    const states = []
    const delays = []
    const uploader = new Uploader(server, {
      wait: async (delay) => delays.push(delay),
      onChange: (uploader) => states.push(uploader.offline)
    })

    server.failures = Array.from({ length: 7 }, () => new ApiError(0))
    await uploader.add([makeFile('a.txt', '0123456789')])

    expect(server.content('a.txt')).toBe('0123456789')
    expect(delays).toEqual([1, 2, 4, 8, 16, 30, 30].map((seconds) => seconds * 1000))
    expect(delays[0]).toBe(RetryDelayMin)
    expect(Math.max(...delays)).toBe(RetryDelayMax)
    expect(states).toContain(true)
    expect(uploader.offline).toBe(false)
  })

  it('moves on to the next file after a final error', async () => {
    const server = new FakeServer()
    const { uploader } = makeUploader(server)

    server.failures = [new ApiError(400)]
    await uploader.add([makeFile('bad.txt', 'abc'), makeFile('good.txt', 'def')])

    expect(uploader.items.map((item) => item.status)).toEqual(['failed', 'done'])
    expect(uploader.items[0].error.status).toBe(400)
    expect(uploader.stopped).toBeUndefined()
  })

  it('stops everything when the disk is full', async () => {
    const server = new FakeServer()
    const { uploader } = makeUploader(server)

    server.failures = [undefined, new ApiError(507)]
    await uploader.add([makeFile('a.txt', 'abc'), makeFile('b.txt', 'def')])

    expect(uploader.items.map((item) => item.status)).toEqual(['failed', 'waiting'])
    expect(uploader.stopped.status).toBe(507)
  })

  it('queues files added while busy', async () => {
    const server = new FakeServer()
    const { uploader } = makeUploader(server)

    const first = uploader.add([makeFile('a.txt', '0123456789')])
    const second = uploader.add([makeFile('b.txt', 'xyz')])
    await Promise.all([first, second])

    // The second call returns right away, the first run picks the new file up
    await new Promise((resolve) => setTimeout(resolve, 0))
    expect(uploader.items.map((item) => item.status)).toEqual(['done', 'done'])
  })
})
