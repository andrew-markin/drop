import { describe, expect, it } from 'vitest'

import { formatSize, fromBase64Url, toBase64Url } from '@/utils'

describe('toBase64Url', () => {
  it('encodes RFC 4648 test vectors without padding', () => {
    const encode = (text) => toBase64Url(new TextEncoder().encode(text))
    expect(encode('')).toBe('')
    expect(encode('f')).toBe('Zg')
    expect(encode('fo')).toBe('Zm8')
    expect(encode('foo')).toBe('Zm9v')
    expect(encode('foob')).toBe('Zm9vYg')
    expect(encode('fooba')).toBe('Zm9vYmE')
    expect(encode('foobar')).toBe('Zm9vYmFy')
  })

  it('uses the URL-safe alphabet', () => {
    expect(toBase64Url(new Uint8Array([0xfb, 0xff, 0xbf]))).toBe('-_-_')
  })
})

describe('fromBase64Url', () => {
  it('decodes what toBase64Url encodes', () => {
    const bytes = Uint8Array.from({ length: 256 }, (_, index) => index)
    for (let length = 0; length <= bytes.length; length++) {
      const slice = bytes.subarray(0, length)
      expect(fromBase64Url(toBase64Url(slice))).toEqual(slice)
    }
  })

  it('rejects padding, other alphabets and impossible lengths', () => {
    expect(fromBase64Url('Zg==')).toBeUndefined()
    expect(fromBase64Url('+/+/')).toBeUndefined()
    expect(fromBase64Url('Zm9vY')).toBeUndefined()
    expect(fromBase64Url('Zm 9v')).toBeUndefined()
    expect(fromBase64Url(undefined)).toBeUndefined()
  })

  it('rejects non-canonical trailing bits', () => {
    expect(fromBase64Url('Zh')).toBeUndefined()
    expect(fromBase64Url('Zm9')).toBeUndefined()
  })
})

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
