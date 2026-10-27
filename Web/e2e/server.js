import { spawn } from 'node:child_process'
import { mkdtemp, readdir, rm } from 'node:fs/promises'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import process from 'node:process'
import { createInterface } from 'node:readline'

import { readSecret } from '@/link'

// Starts the desktop server without its window (Sources/Tests/DropHeadless.cpp, built with the
// tests) with a downloads directory of its own
export async function startServer({ chunkSize = 4 * 1024 * 1024 } = {}) {
  const executable = process.env.DROP_HEADLESS
  if (!executable) throw new Error('Set DROP_HEADLESS to the path of the DropHeadless executable')

  const downloads = await mkdtemp(join(tmpdir(), 'drop-e2e-'))
  const child = spawn(executable, ['--downloads', downloads, '--chunk-size', String(chunkSize)], {
    stdio: ['ignore', 'pipe', 'inherit']
  })

  const firstLine = await new Promise((resolve, reject) => {
    const lines = createInterface({ input: child.stdout })
    lines.once('line', resolve)
    child.once('exit', (code) => reject(new Error(`DropHeadless exited with ${code}`)))
    child.once('error', reject)
  })

  const url = new URL(JSON.parse(firstLine).link)

  return {
    origin: url.origin,
    secret: readSecret(url.hash),
    downloads,
    chunkSize,
    // Requests go to the server the way the page sends them, by path
    fetch: (path, options) => fetch(url.origin + path, options),
    async dropDirectory() {
      const entries = await readdir(downloads)
      return entries.length === 1 ? join(downloads, entries[0]) : undefined
    },
    async stop() {
      child.kill()
      await new Promise((resolve) =>
        child.exitCode !== null ? resolve() : child.once('exit', resolve)
      )
      await rm(downloads, { recursive: true, force: true })
    }
  }
}
