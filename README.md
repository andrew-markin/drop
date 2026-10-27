<h1><img src="Assets/Drop.svg" alt="Drop Logo" width="30"> Drop</h1>

**Drop** moves photos, videos and any other files from a phone, a tablet or another computer to
your computer over the local network. No cables, no cloud, no internet access, and nothing to
install on the phone: scan a QR code and pick the files.

## Features

- **Nothing to install on devices:** the computer serves a small web page, any modern browser
  sends files from it.
- **Any files of any size:** files go in chunks, so even long videos make it, and the page keeps
  the phone screen on while they are sent.
- **Survives a flaky network:** lost connections are retried, and a reloaded page continues
  where it stopped once the same files are picked again.
- **Encrypted end to end:** the link carries a one-time secret that never travels over the
  network, see [Security](#security).
- **Tidy results:** every drop goes to a folder of its own in Downloads, `Drop-YYYY-MM-DD-N`,
  files keep their names and modification times.
- **Progress on both sides:** the page and the window of the application show what is sent.

## How to Use

1. Start **Drop** on the computer. It shows a QR code and the address it listens at.
2. Scan the QR code with a phone connected to the same Wi-Fi or wired network. To send the link
   to a device without a camera, press **Copy Link** and send it in a messenger.
3. On the page that opens, press **Choose Files** and pick photos, videos or other files.
4. Keep the page open until the files are sent. They appear in a `Drop-YYYY-MM-DD-N` folder in
   the Downloads folder of the computer; **Open Folder** shows it.

Every start of Drop is a new drop: a new folder, a new address and a new secret, so links from
earlier starts stop working.

### iPhone and iPad

After you pick photos and videos from the photo library, the picker stays on the screen for a
while before the files start to go. In that time iOS prepares them: copies them out of the
library, downloads originals kept in iCloud, and converts them to the most compatible formats,
so photos arrive as JPEG and videos as H.264. With many photos or long videos this takes minutes.
The page learns about the files only when iOS hands them over, so neither it nor its settings
can make this faster; the picker just has to be left alone until it closes.

The upside is that the files open on any computer, Windows without HEIC and HEVC support
included. To get the originals instead, look for **Options** at the top of the photo picker,
where some iOS versions let you choose the format.

## Security

The page is served over plain HTTP, the only way a phone browser can reach a computer on the
local network without warnings, so Drop encrypts everything itself:

- The link has a random 256-bit secret in its fragment (the part after `#`), which browsers
  never send over the network.
- Every page agrees on keys with the computer through a one-time X25519 exchange mixed with the
  secret, then every request and answer is encrypted and authenticated with
  XChaCha20-Poly1305.
- Devices without the link can neither read the traffic nor upload anything. Someone who learns
  the link later, for example from a messenger history, cannot decrypt traffic recorded earlier.

The page itself still comes over plain HTTP, so an active attacker on the same network could
replace it. Use Drop on networks you trust, such as your home Wi-Fi. The protocol is described in
[docs/PROTOCOL.md](docs/PROTOCOL.md).

## System Requirements

- **Windows:** Windows 10 / 11 (64-bit)
- **Linux (DEB-based):** Ubuntu 22.04 or newer, Debian 12 or newer, and other compatible
  distributions
- **Linux (RPM-based):** Fedora 39 or newer, RHEL 9 / AlmaLinux 9 or newer
- **Devices that send files:** any modern browser, such as Safari on iPhone and iPad or Chrome
  on Android

## Installation

Choose the package for your operating system in the **[Releases](../../releases)** section.

### Windows

Download the installer (`.exe` file), run it and follow the setup wizard. The installer allows
Drop in Windows Firewall on **private** networks. If your home network is marked as public, devices
won't reach Drop: make it private in Windows Settings, *Network & internet*.

### Linux (DEB-based distributions)

    sudo apt install ./drop-*.deb

The package is built on Ubuntu 22.04 and should work on all newer versions of Ubuntu, Debian 12+,
and compatible distributions.

### Linux (RPM-based distributions)

    sudo dnf install ./drop-*.rpm

The package is built on Fedora 39 and should work on Fedora 39+, RHEL 9+, AlmaLinux 9+, and
compatible distributions. If a firewall blocks incoming connections, allow them on the local
network.

### Verifying the Download

Every release includes a `SHA256SUMS` file with checksums of all packages. Download it next to
your package and check that the package was not altered:

- **Linux:**

      sha256sum -c SHA256SUMS --ignore-missing

- **Windows (PowerShell):** compare the output with the line for your file in `SHA256SUMS`:

      Get-FileHash .\Drop-*.exe -Algorithm SHA256

## Building from Sources

You need Qt 6.11 with the HTTP Server module, libsodium 1.0.18 or newer, Node.js 22 or newer and
CMake. Packages are built the same way as in CI:

- **Linux:** `./ReleaseUbuntu.sh` or `./ReleaseFedora.sh` build in Docker images from
  `Dockerfile.ubuntu` and `Dockerfile.fedora`, or locally if `QT_PREFIX` is set in `.env`
  (see `.env.example`).
- **Windows:** install libsodium with `vcpkg install libsodium:x64-windows-static-md`, set
  `QT_PREFIX` and `VCPKG_ROOT` in `.env`, and run `ReleaseWindows.cmd`.

Packages appear in the `Release` folder.

## License

Drop is distributed under the GNU General Public License v3.0, see [LICENSE](LICENSE).
