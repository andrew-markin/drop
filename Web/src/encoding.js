// Base64url without padding (RFC 4648, section 5), as used by the protocol, see docs/PROTOCOL.md

export function toBase64Url(bytes) {
  let binary = ''
  for (const byte of bytes) binary += String.fromCharCode(byte)
  return btoa(binary).replaceAll('+', '-').replaceAll('/', '_').replace(/=+$/, '')
}

// Returns undefined for anything that is not strict base64url without padding
export function fromBase64Url(text) {
  if (typeof text !== 'string' || !/^[A-Za-z0-9_-]*$/.test(text) || text.length % 4 === 1) {
    return undefined
  }
  const binary = atob(text.replaceAll('-', '+').replaceAll('_', '/'))
  const bytes = Uint8Array.from(binary, (char) => char.charCodeAt(0))
  // Reject non-canonical encodings, whose unused trailing bits are not zero
  return toBase64Url(bytes) === text ? bytes : undefined
}
