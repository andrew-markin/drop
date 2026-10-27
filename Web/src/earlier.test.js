import { describe, expect, it } from 'vitest'

import { readEarlier, withoutPicked } from '@/earlier'

describe('readEarlier', () => {
  it('tells how much of every file is received', () => {
    const answer = {
      chunk: 4,
      files: [
        { file: 'A', name: 'a.jpg', size: 10, received: 3, complete: true },
        { file: 'B', name: 'b.mp4', size: 10, received: 1, complete: false },
        { file: 'C', name: 'c.mp4', size: 10, received: 2, complete: false },
        { file: 'D', name: 'empty.txt', size: 0, received: 0, complete: true }
      ]
    }
    expect(readEarlier(answer)).toEqual([
      { name: 'a.jpg', size: 10, sent: 10, complete: true },
      { name: 'b.mp4', size: 10, sent: 4, complete: false },
      { name: 'c.mp4', size: 10, sent: 8, complete: false },
      { name: 'empty.txt', size: 0, sent: 0, complete: true }
    ])
  })

  it('never counts more than the file size', () => {
    // The last chunk is shorter, chunks may come out of order
    const answer = { chunk: 4, files: [{ name: 'a', size: 10, received: 3, complete: false }] }
    expect(readEarlier(answer)[0].sent).toBe(10)
  })
})

describe('withoutPicked', () => {
  it('leaves out files picked again on this page', () => {
    const earlier = [
      { name: 'a.jpg', size: 10 },
      { name: 'a.jpg', size: 11 },
      { name: 'b.jpg', size: 10 }
    ]
    expect(withoutPicked(earlier, [{ name: 'a.jpg', size: 10 }])).toEqual([
      { name: 'a.jpg', size: 11 },
      { name: 'b.jpg', size: 10 }
    ])
    expect(withoutPicked(earlier, [])).toEqual(earlier)
  })
})
