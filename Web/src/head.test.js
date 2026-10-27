import { blake2b } from '@noble/hashes/blake2.js'
import { describe, expect, it } from 'vitest'

import { toBase64Url } from '@/encoding'
import { computeHead, HeadSampleSize } from '@/head'

describe('computeHead', () => {
  it('hashes nothing for an empty file', async () => {
    // BLAKE2b-256 of the empty string, the same value libsodium gives
    const head = await computeHead(new Blob([]))
    expect(toBase64Url(head)).toBe('DldRwCblQ7Loqy6wYJnaodHl30d3j3eH-qtFzfEv46g')
  })

  it('hashes a short file whole', async () => {
    const bytes = new TextEncoder().encode('hello')
    expect(await computeHead(new Blob([bytes]))).toEqual(blake2b(bytes, { dkLen: 32 }))
  })

  it('hashes only the first MiB of a long file', async () => {
    const bytes = Uint8Array.from({ length: HeadSampleSize + 10 }, (_, index) => index % 251)
    const head = await computeHead(new Blob([bytes]))
    expect(head).toEqual(blake2b(bytes.subarray(0, HeadSampleSize), { dkLen: 32 }))

    // Changes after the first MiB do not count
    bytes[HeadSampleSize + 5] ^= 1
    expect(await computeHead(new Blob([bytes]))).toEqual(head)
  })
})
