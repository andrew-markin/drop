import { createHash } from 'node:crypto'
import { readdir, readFile, stat } from 'node:fs/promises'
import { join, relative } from 'node:path'

import { afterEach, describe, expect, it } from 'vitest'

import { Api } from '@/api'
import { randomBytes } from '@/crypto'
import { Uploader } from '@/uploader'

import { startServer } from './server'

const ChunkSize = 64 * 1024

let server
afterEach(async () => {
  await server?.stop()
  server = undefined
})

const noWait = async () => {}

// Deterministic contents, so a file can be made again for a resumed upload
function makeBytes(size, seed = 1) {
  const bytes = new Uint8Array(size)
  let state = seed
  for (let index = 0; index < size; index++) {
    state = (state * 1103515245 + 12345) >>> 0
    bytes[index] = state >>> 24
  }
  return bytes
}

const makeFile = (name, bytes, lastModified = 1700000000000) =>
  new File([bytes], name, { lastModified })
const sha256 = (bytes) => createHash('sha256').update(bytes).digest('hex')

async function upload(files, fetchImpl = server.fetch, secret = server.secret) {
  const uploader = new Uploader(new Api(secret, fetchImpl), { wait: noWait })
  await uploader.add(files)
  return uploader
}

async function listAll(directory) {
  const result = []
  for (const entry of await readdir(directory, { withFileTypes: true, recursive: true })) {
    if (entry.isFile()) result.push(relative(server.downloads, join(entry.parentPath, entry.name)))
  }
  return result.sort()
}

// Loses requests on the way there, or answers on the way back after the server did the work
function makeFlaky(fetchImpl, rate, seed = 7) {
  let state = seed
  const random = () => (state = (state * 1103515245 + 12345) >>> 0) / 2 ** 32
  const stats = { lostRequests: 0, lostAnswers: 0 }
  const flaky = async (path, options) => {
    const roll = random()
    if (roll < rate / 2) {
      stats.lostRequests++
      throw new TypeError('fetch failed')
    }
    const response = await fetchImpl(path, options)
    if (roll < rate) {
      stats.lostAnswers++
      await response.arrayBuffer()
      throw new TypeError('fetch failed')
    }
    return response
  }
  return { flaky, stats }
}

describe('uploads through the desktop server', () => {
  it('stores files whole, under their names, with their times', async () => {
    server = await startServer({ chunkSize: ChunkSize })

    const files = [
      makeFile('IMG_0042.jpg', makeBytes(3 * ChunkSize + 123, 1), 1690000000123),
      makeFile('exact.bin', makeBytes(2 * ChunkSize, 2)),
      makeFile('one chunk.txt', makeBytes(100, 3)),
      makeFile('Фото с моря 🌊.heic', makeBytes(ChunkSize - 1, 4)),
      makeFile('empty.txt', new Uint8Array())
    ]

    const uploader = await upload(files)
    expect(uploader.items.map((item) => item.status)).toEqual(Array(files.length).fill('done'))

    const directory = await server.dropDirectory()
    expect(directory).toMatch(/Drop-\d{4}-\d{2}-\d{2}-1$/)

    for (const file of files) {
      const path = join(directory, file.name)
      const bytes = new Uint8Array(await file.arrayBuffer())
      expect(sha256(await readFile(path)), file.name).toBe(sha256(bytes))
      expect(Math.round((await stat(path)).mtimeMs), file.name).toBe(file.lastModified)
    }

    // Nothing temporary is left behind
    expect((await readdir(directory)).sort()).toEqual(files.map((file) => file.name).sort())
  })

  it('keeps the files of two devices that share names', async () => {
    server = await startServer({ chunkSize: ChunkSize })

    const first = makeFile('IMG_0001.jpg', makeBytes(2 * ChunkSize + 5, 10))
    const second = makeFile('IMG_0001.jpg', makeBytes(2 * ChunkSize + 5, 11))
    await Promise.all([upload([first]), upload([second])])

    const directory = await server.dropDirectory()
    const stored = await Promise.all(
      ['IMG_0001.jpg', 'IMG_0001 (2).jpg'].map(async (name) =>
        sha256(await readFile(join(directory, name)))
      )
    )
    const sent = await Promise.all(
      [first, second].map(async (file) => sha256(new Uint8Array(await file.arrayBuffer())))
    )
    expect(stored.sort()).toEqual(sent.sort())
  })

  it('never writes outside the drop directory', async () => {
    server = await startServer({ chunkSize: ChunkSize })

    const names = [
      '../../evil.sh',
      '..\\..\\evil.bat',
      'CON.txt',
      'a:b?c*.txt',
      '.bashrc',
      '/etc/passwd',
      '..'
    ]
    await upload(names.map((name, index) => makeFile(name, makeBytes(10, index))))

    const directory = relative(server.downloads, await server.dropDirectory())
    expect(await listAll(server.downloads)).toEqual(
      ['File', '_CON.txt', 'abc.txt', 'bashrc', 'evil.bat', 'evil.sh', 'passwd'].map((name) =>
        join(directory, name)
      )
    )
  })

  it('gets through a network that loses requests and answers', async () => {
    server = await startServer({ chunkSize: ChunkSize })

    const { flaky, stats } = makeFlaky(server.fetch, 0.3)
    const files = [
      makeFile('video.mp4', makeBytes(40 * ChunkSize + 77, 20)),
      makeFile('note.txt', makeBytes(5, 21))
    ]

    const uploader = await upload(files, flaky)
    expect(uploader.items.map((item) => item.status)).toEqual(['done', 'done'])

    // Both kinds of losses happened, a lost answer means the same chunk came twice
    expect(stats.lostRequests).toBeGreaterThan(3)
    expect(stats.lostAnswers).toBeGreaterThan(3)

    const directory = await server.dropDirectory()
    for (const file of files) {
      const bytes = new Uint8Array(await file.arrayBuffer())
      expect(sha256(await readFile(join(directory, file.name)))).toBe(sha256(bytes))
    }
  })

  it('resumes after the page is reloaded', async () => {
    server = await startServer({ chunkSize: ChunkSize })

    const makeVideo = () => makeFile('long.mp4', makeBytes(30 * ChunkSize + 9, 30))
    const chunkCount = 31

    // The page goes away after a few chunks: every later request fails for good
    let sentChunks = 0
    const closing = async (path, options) => {
      if (path.includes('/chunk/') && ++sentChunks > 10) return new Response(null, { status: 400 })
      return server.fetch(path, options)
    }
    const first = await upload([makeVideo()], closing)
    expect(first.items[0].status).toBe('failed')

    const { files } = await new Api(server.secret, server.fetch).status()
    expect(files).toHaveLength(1)
    const received = files[0].received
    expect(received).toBeGreaterThanOrEqual(10)
    expect(received).toBeLessThan(chunkCount)

    // A new page, a new session, the same file picked again
    let resumedChunks = 0
    const counting = async (path, options) => {
      if (path.includes('/chunk/')) resumedChunks++
      return server.fetch(path, options)
    }
    const second = await upload([makeVideo()], counting)
    expect(second.items[0].status).toBe('done')
    expect(resumedChunks).toBe(chunkCount - received)

    const bytes = new Uint8Array(await makeVideo().arrayBuffer())
    expect(sha256(await readFile(join(await server.dropDirectory(), 'long.mp4')))).toBe(
      sha256(bytes)
    )
  })

  it('takes nothing from a page with the wrong link', async () => {
    server = await startServer({ chunkSize: ChunkSize })

    const uploader = await upload([makeFile('a.txt', makeBytes(10))], server.fetch, randomBytes(32))
    expect(uploader.items[0].status).toBe('failed')
    expect(uploader.items[0].error.status).toBe(401)
    expect(await readdir(server.downloads)).toEqual([])
  })

  it('sends the default 4 MiB chunks', async () => {
    server = await startServer()

    const file = makeFile('big.bin', makeBytes(9 * 1024 * 1024 + 1, 40))
    await upload([file])

    const bytes = new Uint8Array(await file.arrayBuffer())
    expect(sha256(await readFile(join(await server.dropDirectory(), 'big.bin')))).toBe(
      sha256(bytes)
    )
  })
})
