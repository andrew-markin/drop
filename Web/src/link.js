import { fromBase64Url } from '@/encoding'

export const SecretLength = 32

// The link secret comes in the URL fragment, which the browser never sends over the network:
// http://192.168.0.5:54246/#k=<base64url(k)>
export function readSecret(hash) {
  const params = new URLSearchParams(hash.replace(/^#/, ''))
  const secret = fromBase64Url(params.get('k') ?? '')
  return secret?.length === SecretLength ? secret : undefined
}
