import { computeHead } from '@/crypto'

export const ChunksInFlight = 3
export const RetryDelayMin = 1000 // 1s
export const RetryDelayMax = 30000 // 30s

const sleep = (delay) => new Promise((resolve) => setTimeout(resolve, delay))

// Sends picked files in chunks, a few chunks at a time. The chunks are taken in the order of the
// files, so files arrive one after another, but the next file starts as soon as the chunks of the
// current one are all on their way: small files, a chunk each, do not wait for one another. All
// picked files are announced before the first chunk is sent, so the computer lists the whole drop
// from the start. Network failures are retried for as long as the page is open, so a phone that
// lost Wi-Fi for a while just goes on.
export class Uploader {
  #api
  #onChange
  #wait
  #queue = [] // Files with chunks still to send or to wait for, in the order they were picked
  #running = false
  #announcing = null

  constructor(api, { onChange = () => {}, wait = sleep } = {}) {
    this.#api = api
    this.#onChange = onChange
    this.#wait = wait
    this.items = []
    this.offline = false
    this.stopped = undefined // The error that stopped everything, such as a full disk
  }

  get sentSize() {
    return this.items.reduce((sum, item) => sum + item.sent, 0)
  }

  get totalSize() {
    return this.items.reduce((sum, item) => sum + item.size, 0)
  }

  get busy() {
    return this.#running
  }

  add(files) {
    for (const file of files) {
      const item = { file, name: file.name, size: file.size, sent: 0, status: 'waiting' }
      this.items.push(item)
      this.#queue.push(item)
    }
    this.#changed()
    return this.#run()
  }

  async #run() {
    if (this.#running) return
    this.#running = true
    try {
      await Promise.all(Array.from({ length: ChunksInFlight }, () => this.#work()))
    } finally {
      this.#running = false
      this.#changed()
    }

    // Files added right when the workers were done
    if (this.#hasWork()) await this.#run()
  }

  async #work() {
    for (let next = await this.#nextChunk(); next; next = await this.#nextChunk()) {
      const { item, index } = next
      const { file, chunk } = item.upload
      item.inFlight++
      item.status = 'uploading'
      this.#changed()

      try {
        const start = index * chunk
        const bytes = new Uint8Array(await item.file.slice(start, start + chunk).arrayBuffer())
        await this.#retrying(() => this.#api.sendChunk(file, index, bytes))
        if (item.status !== 'failed') item.sent += bytes.length
      } catch (error) {
        // Chunks of the file already on their way finish, no more are taken
        if (item.status !== 'failed') {
          item.pending = []
          this.#dequeue(item)
          this.#fail(item, error)
        }
      } finally {
        item.inFlight--
        if (item.status === 'uploading' && item.pending.length === 0 && item.inFlight === 0) {
          item.status = 'done'
          this.#dequeue(item)
        }
        this.#changed()
      }
    }
  }

  // The first chunk not taken yet, of the first file that has one
  async #nextChunk() {
    for (;;) {
      if (this.stopped) return null

      const item = this.#queue.find((item) => !item.upload || item.pending.length > 0)
      if (!item) return null

      if (item.upload) return { item, index: item.pending.shift() }
      await this.#announceQueued()
    }
  }

  #hasWork() {
    return !this.stopped && this.#queue.some((item) => !item.upload || item.pending.length > 0)
  }

  // Announces every file not announced yet, once for all the workers waiting for it
  #announceQueued() {
    this.#announcing ??= (async () => {
      // A copy, as announcing takes failed and finished files out of the queue
      for (const item of this.#queue.filter((item) => !item.upload)) {
        if (this.stopped) break
        await this.#announce(item)
      }
    })().finally(() => {
      this.#announcing = null
    })
    return this.#announcing
  }

  async #announce(item) {
    try {
      item.status = 'preparing'
      this.#changed()

      const head = await computeHead(item.file)
      const upload = await this.#retrying(() =>
        this.#api.announce({
          name: item.name,
          size: item.size,
          modified: item.file.lastModified,
          head
        })
      )

      const { chunk, received } = upload
      const chunkCount = Math.ceil(item.size / chunk)
      const chunkSize = (index) => Math.min(chunk, item.size - index * chunk)
      const done = new Set(received)

      item.sent = received.reduce((sum, index) => sum + chunkSize(index), 0)
      item.pending = []
      for (let index = 0; index < chunkCount; index++) {
        if (!done.has(index)) item.pending.push(index)
      }
      item.inFlight = 0
      item.upload = upload

      // An empty file is complete right after the announce, and so is one sent before a reload
      if (item.pending.length === 0) {
        item.status = 'done'
        this.#dequeue(item)
      } else {
        item.status = 'waiting'
      }
    } catch (error) {
      this.#dequeue(item)
      this.#fail(item, error)
    }
    this.#changed()
  }

  #dequeue(item) {
    const index = this.#queue.indexOf(item)
    if (index >= 0) this.#queue.splice(index, 1)
  }

  #fail(item, error) {
    item.status = 'failed'
    item.error = error
    // Nothing else fits either once the disk is full
    if (error.status === 507) this.stopped = error
  }

  async #retrying(action) {
    for (let attempt = 0; ; attempt++) {
      try {
        const result = await action()
        if (this.offline) {
          this.offline = false
          this.#changed()
        }
        return result
      } catch (error) {
        if (!error.retryable) throw error
        if (!this.offline) {
          this.offline = true
          this.#changed()
        }
        await this.#wait(Math.min(RetryDelayMin * 2 ** attempt, RetryDelayMax))
      }
    }
  }

  #changed() {
    this.#onChange(this)
  }
}
