# Notes for Claude

Drop moves files from phones and other devices to a computer over the local network. The desktop
application (C++, Qt 6 Widgets, `Sources/`) runs an HTTP server that serves the web client (Vue 3
and Quasar built with Vite, `Web/`). The protocol between them is in `docs/PROTOCOL.md`; both
sides must follow it exactly. The desktop part follows the style of
https://github.com/andrew-markin/shprot, the web part the style of
https://github.com/andrew-markin/mult.

## Workflow

- Commit straight to `main` in small commits split by meaning, with a short English subject in
  the style of the history and no Co-Authored-By or other attribution.
- `SPECS.md` is the user's own notes and is never committed (it is listed in `.git/info/exclude`).
- Before committing web changes run `npm run lint`, `npm test` and `npm run build` in `Web/`.
- The desktop build runs `npm ci` (when `package-lock.json` changes) and `npm run build` in `Web/`
  and compiles the result into the `:/Web` resources, so it needs Node.js too.
- The desktop build needs libsodium 1.0.18 or newer (`libsodium-dev` on Ubuntu) found through
  pkg-config, or through vcpkg on Windows.
- Both sides check their cryptography against `docs/vectors/protocol.json`. It is made by
  `docs/vectors/generate.py` with PyNaCl (`python3-nacl`), a third implementation; run it only
  when the protocol changes.
- Before committing desktop changes build with tests and run them:

  ```bash
  cmake -S Sources -B Builds/Debug -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/gcc_64 -DCMAKE_BUILD_TYPE=Debug -DDROP_BUILD_TESTS=ON
  cmake --build Builds/Debug --parallel && ctest --test-dir Builds/Debug --output-on-failure
  ```

  `ctest` also runs the end-to-end tests of `Web/e2e`: the web client code against
  `DropHeadless`, the server without the window that is built only with the tests. Alone they
  run with `DROP_HEADLESS=<path to DropHeadless> npm run test:e2e` in `Web/`.

## Decisions to keep

- The link secret `k` lives only in the URL fragment and in memory, never in logs, files or
  request URLs.
- The page comes over plain HTTP, so `crypto.subtle` and the Wake Lock API are not available in
  it: cryptography uses `@noble/*` packages, and the screen is kept on with a looping video.
- libsodium 1.0.18 is the oldest supported version (Ubuntu 22.04 builds), so its newer functions
  such as `crypto_kdf_hkdf_sha256_*` are not used.
- The server listens on a random free port, a new one on every start.
