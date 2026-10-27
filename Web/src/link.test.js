import { describe, expect, it } from 'vitest'

import { toBase64Url } from '@/encoding'
import { readSecret, SecretLength } from '@/link'

describe('readSecret', () => {
  const secret = Uint8Array.from({ length: SecretLength }, (_, index) => index * 7)
  const encoded = toBase64Url(secret)

  it('reads the secret from the fragment', () => {
    expect(readSecret(`#k=${encoded}`)).toEqual(secret)
    expect(readSecret(`k=${encoded}`)).toEqual(secret)
  })

  it('ignores other parameters', () => {
    expect(readSecret(`#x=1&k=${encoded}&y=2`)).toEqual(secret)
  })

  it('rejects a missing, malformed or wrong length secret', () => {
    expect(readSecret('')).toBeUndefined()
    expect(readSecret('#')).toBeUndefined()
    expect(readSecret('#k=')).toBeUndefined()
    expect(readSecret(`#k=${encoded}=`)).toBeUndefined()
    expect(readSecret(`#k=${encoded.slice(0, -2)}`)).toBeUndefined()
    expect(readSecret(`#k=${toBase64Url(new Uint8Array(SecretLength + 1))}`)).toBeUndefined()
  })
})
