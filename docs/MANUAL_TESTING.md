# Manual testing

Automated tests cover the protocol, the server and the web client code, but not real phones,
installers and desktops. Go through this list before a release, with packages from the CI run of
the release commit (Actions, the run, *Artifacts*).

## Desktop

For each of Windows, Ubuntu and Fedora:

- [ ] The package installs, Drop is in the start menu or the application list with its icon.
- [ ] Drop starts and shows a QR code, the address is in the local network (`192.168.x.x` or
      similar), not a Docker or VPN one.
- [ ] Windows only: no firewall prompt on the first start, `netsh advfirewall firewall show rule
      name=Drop` lists a rule for private networks.
- [ ] Windows only: on a network marked as public, the phone cannot open the page, and after a
      minute Drop hints at making the network private.
- [ ] The window and the notification are in Russian when the system language is Russian.
- [ ] Uninstalling removes Drop (on Windows, the firewall rule too).

## Phones

For each of an iPhone (Safari) and an Android phone (Chrome), on the same Wi-Fi as the computer:

- [ ] Scanning the QR code with the camera opens the page, it says nothing about a wrong link.
- [ ] **Choose Files** offers the photo library, several photos and videos can be picked.
- [ ] Files arrive whole into `Downloads/Drop-YYYY-MM-DD-1`, open fine on the computer, and keep
      their modification times.
- [ ] The screen stays on during a long upload (a few minutes of video) without touching it.
- [ ] Turning Wi-Fi off for a while in the middle of an upload shows the connection banner, the
      upload goes on after Wi-Fi is back.
- [ ] Reloading the page in the middle of an upload lists the files already on the computer,
      picking the unfinished file again continues it rather than starting over.
- [ ] The page is in Russian when the phone language is Russian.
- [ ] When the upload is over, the computer shows a notification, clicking it opens the folder.

## Two devices at once

- [ ] Two phones send files at the same time, all files arrive, same names get ` (2)`.
- [ ] A link sent from the computer in a messenger opens the page on another computer.
