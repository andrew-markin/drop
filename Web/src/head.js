import { blake2b } from '@noble/hashes/blake2.js'

// Matches a resumed upload to the same file, see "Announce a file" in docs/PROTOCOL.md
export const HeadSampleSize = 1024 * 1024
export const HeadSize = 32

export async function computeHead(file) {
  const sample = new Uint8Array(await file.slice(0, HeadSampleSize).arrayBuffer())
  return blake2b(sample, { dkLen: HeadSize })
}
