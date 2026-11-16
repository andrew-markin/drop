---
# The text of the page. Every language has the same structure, "id" values are not translated.
# Texts may contain inline Markdown, {version} is replaced with the latest release version.
title: "Drop — send files from a phone to a computer over Wi-Fi"
description: "Free app for Windows and Linux that receives photos, videos and any other files from phones and other devices over the local network. Nothing to install on the phone, no cloud, no cables, encrypted end to end."
readme: "README.md"

ui:
  skip: "Skip to content"
  nav_features: "Features"
  nav_compare: "Why Drop"
  nav_download: "Download"
  nav_faq: "FAQ"
  language: "Language"
  github: "Source code on GitHub"

hero:
  title: "Files from your phone to your computer, without the cloud"
  lead: "Start Drop on the computer, scan the QR code with the phone and pick the files. Photos, videos and documents of any size go straight over your Wi-Fi into the Downloads folder — no app on the phone, no account, no internet."
  download: "Download"
  download_hint: "Version {version} for Windows and Linux"
  facts:
    - "Free and open source"
    - "Nothing to install on the phone"
    - "Encrypted end to end"
  screenshot_alt: "The Drop window receiving files from a phone"

features:
  title: "Made for moving many big files at once"
  items:
    - id: phone
      title: "Nothing to install on devices"
      text: "The computer serves a small web page. Any modern browser sends files from it: Safari on iPhone and iPad, Chrome on Android, a browser on another laptop."
    - id: size
      title: "Any files of any size"
      text: "Files go in chunks, several at a time, so hours of video make it as well as a single document. Drop never recompresses anything."
    - id: network
      title: "Survives a flaky network"
      text: "Lost connections are retried on their own. If the page was closed, open the link again and pick the same files: they continue where they stopped."
    - id: lock
      title: "Encrypted end to end"
      text: "The link carries a one-time secret that never travels over the network. Devices without the link can neither read the traffic nor send anything."
    - id: folder
      title: "Tidy results"
      text: "Every drop goes to a folder of its own in Downloads, `Drop-YYYY-MM-DD-N`. Files keep their names and modification times."
    - id: progress
      title: "Progress on both sides"
      text: "The page and the window show every file as it goes, the window shows the time left and tells with a notification when the drop is complete."
    - id: offline
      title: "No internet needed"
      text: "Files travel only inside your local network. Nothing is uploaded anywhere, so it works without internet access and with no size limits but your disk."
    - id: languages
      title: "English and Russian"
      text: "The window and the page follow the language of the computer and of the phone."

compare:
  title: "Why not a messenger or the cloud?"
  lead: "Sending files to yourself through a messenger or a cloud drive works for a photo or two. With a few hundred photos or long videos it turns into waiting, limits and lost quality."
  other: "Messenger or cloud"
  rows:
    - case: "Photo and video quality"
      other: "Often compressed on the way"
      drop: "Sent as they are, never recompressed"
    - case: "Large videos"
      other: "File size and storage limits"
      drop: "No limits but the free disk space"
    - case: "Speed"
      other: "Up to the internet and back down"
      drop: "As fast as your Wi-Fi"
    - case: "Internet access"
      other: "Required"
      drop: "Not needed, the local network is enough"
    - case: "Where the files go"
      other: "Through someone else's servers"
      drop: "Straight to your computer, encrypted"
    - case: "On the computer"
      other: "Download and sort out by hand"
      drop: "Already in a dated folder in Downloads"
  note: "A cable works too, but needs the right cable at hand, and an iPhone also needs extra software on Windows."

uses:
  title: "What it's good for"
  items:
    - id: photos
      title: "Photos off the phone"
      text: "Move a vacation or a family event to the computer in one go and free up space on the phone."
    - id: video
      title: "Long videos"
      text: "Gigabytes of 4K video go without compression and without waiting for an upload to the cloud."
    - id: laptop
      title: "Files from guests"
      text: "Friends send their photos from their own phones: they only scan the QR code, nothing to install or sign up for."

how:
  title: "Three steps, no setup"
  steps:
    - title: "Start Drop"
      text: "Run Drop on the computer. If it is on several networks, choose the one the phone is on."
    - title: "Scan the QR code"
      text: "Press **Show QR** and scan it with the phone camera. Devices without a camera get the link through **Copy Link**."
    - title: "Pick the files"
      text: "Tap the drop button on the page and pick photos, videos or other files. They appear in the Downloads folder as they come."
  guide: "Detailed guide and troubleshooting"
  page_alt: "The page on a phone sending files to the computer"
  page_caption: "The page on the phone shows what is sent and what is left."

download:
  title: "Download Drop"
  lead: "Version {version}. Free, open source, no accounts or subscriptions."
  windows:
    title: "Windows"
    text: "Windows 10 and 11, 64-bit. The installer allows Drop in Windows Firewall on private networks."
    button: "Installer (.exe)"
  deb:
    title: "Ubuntu, Debian"
    text: "Ubuntu 22.04+, Debian 12+ and compatible distributions."
    button: "Package (.deb)"
  rpm:
    title: "Fedora, RHEL"
    text: "Fedora 39+, RHEL 9+, AlmaLinux 9+ and compatible distributions."
    button: "Package (.rpm)"
  requirements: "Install Drop only on the computer that receives files. Devices that send them need nothing but a modern browser and the same local network."
  verify: "Every release includes a `SHA256SUMS` file to check that the download wasn't altered."
  all_releases: "All releases and changes"

faq:
  title: "Questions and answers"
  items:
    - q: "Do I need to install anything on the phone?"
      a: "No. The phone opens a web page served by Drop on the computer, the camera app opens it from the QR code. Any modern browser works."
    - q: "Does it need internet access?"
      a: "No. The phone and the computer only have to be on the same local network, for example on the same Wi-Fi router. Nothing goes through the internet."
    - q: "Is it safe over plain HTTP?"
      a: "Everything the page and the computer send each other is encrypted and authenticated with keys made from a one-time secret in the link, which browsers never send over the network. The page itself still comes over HTTP, so use Drop on networks you trust, such as your home Wi-Fi."
    - q: "The phone doesn't open the page. What's wrong?"
      a: "Most often the phone is on another network: a guest Wi-Fi, mobile data or a VPN. On Windows, the home network also has to be marked as private in the network settings, as the firewall lets Drop in only on private networks."
    - q: "Why do photos from an iPhone arrive as JPEG?"
      a: "iOS converts photos and videos to the most compatible formats when they are picked in the browser, so they open on any computer. Some iOS versions let you choose originals in **Options** at the top of the photo picker."
    - q: "Does the phone screen have to stay on?"
      a: "On Android the page keeps the screen on while files are sent. iPhone and iPad lock the screen as usual: sending pauses after a while, unlock the phone and it continues by itself."
    - q: "Can several phones send at once?"
      a: "Yes. Every device that has the link can send files at the same time. Different files with the same name are saved side by side as `name (2)`."
    - q: "Is it free?"
      a: "Yes. Drop is free and open source software under the GNU GPL v3 license, with no accounts, subscriptions or ads."
    - q: "Is there a macOS version?"
      a: "Not yet. Drop currently runs on Windows 10 / 11 and Linux. A Mac can send files to it from a browser, though."

footer:
  license: "Drop is free software licensed under the GNU GPL v3."
  source: "Source code"
  issues: "Report a problem"
---
