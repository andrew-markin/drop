import { computeHead } from '@/head'

export const ChunksInFlight = 3
export const RetryDelayMin = 1000 // 1s
export const RetryDelayMax = 30000 // 30s

const sleep = (delay) => new Promise((resolve) => setTimeout(resolve, delay))

// Sends picked files one after another in chunks, a few chunks at a time. Network failures are
// retried for as long as the page is open, so a phone that lost Wi-Fi for a while just goes on.
export class Uploader {
  #api
  #onChange
  #wait
  #queue = []
  #running = false

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
      while (this.#queue.length > 0 && !this.stopped) {
        await this.#upload(this.#queue.shift())
      }
    } finally {
      this.#running = false
      this.#changed()
    }
  }

  async #upload(item) {
    try {
      item.status = 'preparing'
      this.#changed()

      const head = await computeHead(item.file)
      const { file, chunk, received } = await this.#retrying(() =>
        this.#api.announce({
          name: item.name,
          size: item.size,
          modified: item.file.lastModified,
          head
        })
      )

      const chunkCount = Math.ceil(item.size / chunk)
      const chunkSize = (index) => Math.min(chunk, item.size - index * chunk)
      const done = new Set(received)
      item.sent = [...done].reduce((sum, index) => sum + chunkSize(index), 0)
      item.status = 'uploading'
      this.#changed()

      const pending = []
      for (let index = 0; index < chunkCount; index++) {
        if (!done.has(index)) pending.push(index)
      }

      let aborted = false
      const worker = async () => {
        try {
          while (pending.length > 0 && !aborted) {
            const index = pending.shift()
            const start = index * chunk
            const bytes = new Uint8Array(await item.file.slice(start, start + chunk).arrayBuffer())
            await this.#retrying(() => this.#api.sendChunk(file, index, bytes))
            item.sent += bytes.length
            this.#changed()
          }
        } catch (error) {
          aborted = true // The other workers stop after their current chunk
          throw error
        }
      }

      const workers = Array.from({ length: Math.min(ChunksInFlight, pending.length) }, worker)
      const results = await Promise.allSettled(workers)
      const failure = results.find((result) => result.status === 'rejected')
      if (failure) throw failure.reason

      item.status = 'done'
    } catch (error) {
      item.status = 'failed'
      item.error = error
      // Nothing else fits either once the disk is full
      if (error.status === 507) this.stopped = error
    }
    this.#changed()
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
