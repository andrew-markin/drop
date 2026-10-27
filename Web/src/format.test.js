import { describe, expect, it } from 'vitest'

import { formatSize } from '@/format'

describe('formatSize', () => {
  it('picks a unit that keeps the number short', () => {
    expect(formatSize(0, 'en')).toBe('0 kB')
    expect(formatSize(20, 'en')).toBe('0.1 kB')
    expect(formatSize(300, 'en')).toBe('0.3 kB')
    expect(formatSize(1000, 'en')).toBe('1 kB')
    expect(formatSize(1536, 'en')).toBe('1.5 kB')
    expect(formatSize(9_500_000, 'en')).toBe('9.5 MB')
    expect(formatSize(123_456_789, 'en')).toBe('123 MB')
    expect(formatSize(4_200_000_000, 'en')).toBe('4.2 GB')
    expect(formatSize(5e15, 'en')).toBe('5,000 TB')
  })
})
