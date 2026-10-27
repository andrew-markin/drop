# Drop protocol, version 1

This document describes how the web page (the client) talks to the desktop application (the
server). Both implementations must follow it byte for byte and are checked against the same test
vectors, `docs/vectors/protocol.json`, made by a third implementation (`docs/vectors/generate.py`).

## Goals and threat model

The server is reachable only over plain HTTP on the local network, so the protocol encrypts and
authenticates everything itself.

Protected against:

- Anyone on the local network who does not know the link: they can neither read the traffic nor
  upload anything.
- Anyone who learns the link (for example, from a messenger history) but only listens to the
  network: the session key also depends on a one-time X25519 exchange.
- Anyone who finds the link later and has recorded the traffic: one-time X25519 keys are gone
  once the session ends (forward secrecy).

Not protected against:

- An active attacker on the local network who replaces the page itself. The page comes over
  plain HTTP, so substituted code can read the secret from the link and do anything with it.
- An active attacker who knows the link and intercepts the connection during the drop.

## Primitives

| Purpose          | Primitive                       | libsodium                                  | JavaScript                   |
| ---------------- | ------------------------------- | ------------------------------------------ | ---------------------------- |
| Key exchange     | X25519                          | `crypto_scalarmult`                        | `@noble/curves/ed25519`      |
| Key derivation   | Keyed BLAKE2b                   | `crypto_generichash`                       | `@noble/hashes/blake2b`      |
| Encryption       | XChaCha20-Poly1305 (IETF)       | `crypto_aead_xchacha20poly1305_ietf_*`     | `@noble/ciphers/chacha`      |
| Random bytes     | CSPRNG                          | `randombytes_buf`                          | `crypto.getRandomValues`     |

All of these work with libsodium 1.0.18 (Ubuntu 22.04 and 24.04) and in browsers over plain HTTP,
where `crypto.subtle` is not available.

## Encoding

- Binary values inside JSON are base64url without padding (RFC 4648, section 5).
- JSON text is UTF-8.
- `‖` means concatenation of byte strings. Text labels are ASCII without a terminating zero.
- Integers inside binary structures are unsigned little-endian of the stated width: `u32`, `u64`.

## Link secret

On every start the server generates a random 32-byte secret `k`. It never leaves the server over
the network, it is only shown to the user inside the link:

```
http://192.168.0.5:54246/#k=<base64url(k)>
```

The browser never sends the fragment (the part after `#`) to the server. The page reads `k` from
`location.hash` and keeps it for the lifetime of the tab, so reloading the page keeps it too.

Every server start uses a new `k`, a new port and a new drop directory.

## Session

The page opens a session when it loads, and opens a new one whenever the server forgets the old
one (see Errors).

### Hello

The client generates a one-time X25519 key pair `(cs, cp)` and sends:

```
POST /api/session
Content-Type: application/json

{ "key": "<base64url(cp)>", "proof": "<base64url(hello)>" }
```

where

```
hello = BLAKE2b-256(key = k, message = "Drop/1 hello" ‖ cp)
```

The proof lets the server turn down strangers before it does any work on their behalf. A replayed
hello is useless without `cs`.

The server checks the proof (constant time), generates its own one-time key pair `(ss, sp)` and a
random 16-byte session identifier `sid`, derives the keys and answers:

```
200 OK
Content-Type: application/json

{ "session": "<base64url(sid)>", "key": "<base64url(sp)>", "proof": "<base64url(confirm)>" }
```

### Keys

Both sides compute

```
shared = X25519(cs, sp) = X25519(ss, cp)
keys   = BLAKE2b-512(key = k, message = "Drop/1 keys" ‖ shared ‖ cp ‖ sp)
up     = keys[0..32]    (client to server)
down   = keys[32..64]   (server to client)
```

An all-zero `shared` (a low-order public key) is rejected by both sides.

`confirm` from the hello answer is a sealed message (see below) from the server to the client
with an empty body and the label `"Drop/1 confirm"`. It shows the client that the server knows
`k`, so a wrong or tampered link is reported right away instead of failing later.

The server drops `ss` right after the hello. Both sides keep `up` and `down` only in memory.

## Sealed messages

Every request after the hello carries a sealed body, and every answer to it carries a sealed body
in return:

```
sealed = nonce ‖ XChaCha20-Poly1305(key, nonce, plaintext, ad)
```

- `nonce` is 24 random bytes, new for every message. Random nonces need no counters, so retries,
  parallel requests and resumed uploads can never reuse one.
- `key` is `up` for requests and `down` for answers.
- `ad` (associated data) binds the message to its place, so it cannot be moved anywhere else:

  ```
  request ad = "Drop/1 request" ‖ sid ‖ method ‖ " " ‖ path
  answer ad  = "Drop/1 answer" ‖ request nonce
  confirm ad = "Drop/1 confirm" ‖ sid ‖ cp ‖ sp
  ```

  `method` and `path` are exactly as sent in the request line, for example
  `POST /api/session/<sid>/chunk/<file>/<index>`. Paths hold only base64url characters, digits and
  slashes, so they never need percent-encoding.

Requests and answers are `Content-Type: application/octet-stream`. Unless stated otherwise, their
plaintext is JSON.

A request that cannot be opened gets `401` with no body. A replayed request is harmless: every
operation below is idempotent.

## Operations

All paths start with `/api/session/<sid>`, where `<sid>` is base64url.

### Announce a file

```
POST /api/session/<sid>/file
```

```json
{ "name": "IMG_0042.jpg", "size": 3145728, "modified": 1761580800000, "head": "<base64url>" }
```

- `name` is the file name as the browser reports it. The server never trusts it as a path.
- `size` is in bytes, `modified` is milliseconds since the Unix epoch (`File.lastModified`).
- `head` is `BLAKE2b-256` (unkeyed) of the first MiB of the file (or of the whole file if it is
  shorter), so a resumed upload can be matched to the same file even if it was picked again after
  a page reload. It does not depend on the chunk size, which the client learns only from the
  answer.

The answer:

```json
{ "file": "<base64url(16 bytes)>", "chunk": 4194304, "received": [0, 1, 5] }
```

- `file` identifies the upload. The same `name`, `size`, `modified` and `head` within one server
  run give the same `file`, whichever session announces it.
- `chunk` is the chunk size the server wants, the same for the whole upload.
- `received` lists the indexes of chunks the server already has, empty for a new file.

An empty file has no chunks: its `head` is the hash of nothing, and it is complete right after the
announce.

### Send a chunk

```
POST /api/session/<sid>/chunk/<file>/<index>
```

The plaintext is the raw bytes of the chunk number `index` (zero-based). Every chunk is exactly
`chunk` bytes long, except the last one, which holds the rest of the file.

The answer:

```json
{ "received": 6, "complete": false }
```

- `received` is the number of chunks of this file the server has.
- `complete` turns `true` once every chunk is written and the file is in place under its final
  name.

Sending a chunk the server already has does nothing and gets the same answer. Chunks may come in
any order and in parallel.

### Status

```
POST /api/session/<sid>/status
```

The plaintext is empty. The answer lists the uploads of this server run from all devices, so a
reloaded page can show what is already done:

```json
{
  "chunk": 4194304,
  "files": [{ "file": "<base64url>", "name": "IMG_0042.jpg", "size": 3145728, "received": 6, "complete": false }]
}
```

`chunk` is the chunk size, as in the announce answer, so the page can tell how much of a file is
received from the number of chunks.

## Storage

- The drop directory `Drop-YYYY-MM-DD-N` is created in the system downloads directory when the
  first chunk of the run arrives. `N` starts at 1 and is the first number not taken yet that day.
- A file is written to a hidden temporary name inside the drop directory and renamed when it is
  complete. If the name is taken, a suffix is added: `IMG_0042 (2).jpg`.
- Names are reduced to their last path component, stripped of control characters and of
  characters that Windows does not allow, and of leading dots and trailing dots and spaces.
  Windows device names (`CON`, `NUL`, `COM1` and so on) get a `_` prefix, names longer than
  200 bytes in UTF-8 are shortened keeping the extension, and an empty result becomes `File`.
- The modification time of the complete file is set to `modified`.

## Errors

| Status | Meaning                                                                      | Client reaction                       |
| ------ | ---------------------------------------------------------------------------- | ------------------------------------- |
| `400`  | Malformed request: bad JSON, wrong chunk length, index out of range          | Bug, report it                        |
| `401`  | Wrong hello proof, or a sealed request does not open                         | The link is wrong, ask to rescan it   |
| `404`  | Unknown session or file (for example, the server was restarted)              | Open a new session and announce again |
| `413`  | Body is larger than `chunk` plus the sealing overhead                        | Bug, report it                        |
| `507`  | No space left on the disk                                                    | Stop and tell the user                |

Network failures and `5xx` other than `507` are retried with a growing delay. The client resumes
from the `received` list of the announce answer.

## Limits

- Chunk size: 4 MiB, decided by the server.
- Request body: at most `chunk + 40` bytes (nonce and tag).
- The server keeps at most 64 sessions and forgets the least recently used one beyond that.
