#!/bin/bash -e

# Takes README screenshots: the window of Drop in a VirtualBox VM with Ubuntu GNOME desktop in
# light and dark themes (Assets/Window-Light.png, Assets/Window-Dark.png) and the page on a phone
# sending the same files (Assets/Page.png). Image size in README is updated too.
#
# Usage: ./TakeScreenshots.sh [path/to/drop.deb]
#
# Guest passwords are never needed: the package is unpacked into the guest home directory and
# runs with its own settings and Downloads folder, so the guest's own Drop is not touched. The
# guest theme is restored on exit. The page is opened in headless Chrome on the host, which
# reaches Drop in the guest through an SSH tunnel and picks generated files, so the window and
# the page show one real drop. Chrome is closed right before the window is taken, so both show
# the same progress.
#
# One-time VM preparation (Ubuntu 26.04 GNOME, Wayland session), the same VM serves Shprot:
#
# 1. In the guest, install tools and keep the desktop quiet and static:
#      sudo apt install -y openssh-server xdotool python3-gi gir1.2-atspi-2.0 gir1.2-gtk-3.0
#      gsettings set org.gnome.desktop.screensaver lock-enabled false
#      gsettings set org.gnome.desktop.session idle-delay 0
#      gsettings set org.gnome.desktop.notifications show-banners false
#      gsettings set org.gnome.desktop.interface enable-animations false
#    and turn on Settings > Users > Automatic Login.
#
#    The host needs ImageMagick, Node.js 22 or newer and Google Chrome.
#
# 2. On the host, create a key for the VM and forward SSH to the guest
#    (controlvm works on a running VM, use modifyvm --natpf1 on a stopped one):
#      ssh-keygen -t ed25519 -N "" -f ~/.ssh/drop_vm_ed25519
#      VBoxManage controlvm "Ubuntu Development" natpf1 "ssh,tcp,127.0.0.1,2222,,22"
#
# 3. In the guest, authorize the key (copy the .pub file via a shared folder or USB):
#      mkdir -p ~/.ssh && chmod 700 ~/.ssh
#      cat drop_vm_ed25519.pub >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
#
# 4. Check from the host:
#      ssh -i ~/.ssh/drop_vm_ed25519 -p 2222 developer@127.0.0.1 true
#
# The VM must be running with the user logged in. Other VM names, users, ports or keys are set
# in .env (see .env.example).

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Load environment variables from .env file if exists
if [ -f ".env" ]; then
    set -a  # Automatically export all variables
    source .env
    set +a
fi

VM_NAME="${SCREENSHOTS_VM:-Ubuntu Development}"
SSH_TARGET="${SCREENSHOTS_SSH_TARGET:-developer@127.0.0.1}"
SSH_PORT="${SCREENSHOTS_SSH_PORT:-2222}"
SSH_KEY="${SCREENSHOTS_SSH_KEY:-$HOME/.ssh/drop_vm_ed25519}"
CHROME="${SCREENSHOTS_CHROME:-google-chrome}"

PACKAGE="$1"

if [ -z "$PACKAGE" ]; then
    PACKAGE=$(ls -t Release/drop-*.deb 2>/dev/null | head -1)
fi

if [ ! -f "$PACKAGE" ]; then
    echo "ERROR: DEB package not found. Run ./ReleaseUbuntu.sh or pass the package path."
    exit 1
fi

SSH_OPTIONS=(-i "$SSH_KEY" -p "$SSH_PORT" -o BatchMode=yes)
SCP_OPTIONS=(-q -i "$SSH_KEY" -P "$SSH_PORT" -o BatchMode=yes)

# Runs a command in the guest with the environment of its GNOME session
guest() {
    ssh "${SSH_OPTIONS[@]}" "$SSH_TARGET" "
        while IFS= read -r -d '' v; do export \"\$v\"; done < /proc/\$(pgrep -u \$USER -x gnome-shell | head -1)/environ
        export DISPLAY=:0 XAUTHORITY=\$(ls /run/user/\$(id -u)/.mutter-Xwaylandauth.* | head -1)
        export XDG_CONFIG_HOME=~/DropScreenshots/Config XDG_DATA_HOME=~/DropScreenshots/Data XDG_CACHE_HOME=~/DropScreenshots/Cache
        $1"
}

# Prepare the application in the guest, with a Downloads folder of its own

guest 'rm -rf ~/DropScreenshots && mkdir -p ~/DropScreenshots/Config ~/DropScreenshots/Package ~/DropScreenshots/Downloads
       echo "XDG_DOWNLOAD_DIR=\"$HOME/DropScreenshots/Downloads\"" > ~/DropScreenshots/Config/user-dirs.dirs'
scp "${SCP_OPTIONS[@]}" "$PACKAGE" "$SSH_TARGET:DropScreenshots/drop.deb"
guest 'cd ~/DropScreenshots && dpkg-deb -x drop.deb Package'

TEMPORARY=$(mktemp -d)
TUNNEL_PID=

# Files to send: photos, two videos and a document, empty inside, as only their names and sizes
# are seen
mkdir "$TEMPORARY/Files"
FILES=()
for FILE in "IMG_2041.HEIC 3214567" "IMG_2042.HEIC 2876543" "IMG_2043.HEIC 4632109" "IMG_2044.MOV 312456789" \
            "IMG_2045.HEIC 3398765" "IMG_2046.MOV 168234567" "Boarding pass.pdf 214321"; do
    NAME=${FILE% *}
    truncate -s "${FILE##* }" "$TEMPORARY/Files/$NAME"
    FILES+=("$TEMPORARY/Files/$NAME")
done

stop_drop() {
    guest 'pkill -f "^$HOME/DropScreenshots/Package/opt/Drop/Drop" || true
           for i in $(seq 20); do pgrep -f "^$HOME/DropScreenshots/Package/opt/Drop/Drop" >/dev/null || break; sleep 0.5; done'

    if [ -n "$TUNNEL_PID" ]; then
        kill "$TUNNEL_PID" 2>/dev/null || true
        wait "$TUNNEL_PID" 2>/dev/null || true
        TUNNEL_PID=
    fi
}

# Remember guest settings and restore them on exit

ORIGINAL_COLOR_SCHEME=$(guest "gsettings get org.gnome.desktop.interface color-scheme")
ORIGINAL_GTK_THEME=$(guest "gsettings get org.gnome.desktop.interface gtk-theme")

cleanup() {
    stop_drop
    guest "gsettings set org.gnome.desktop.interface color-scheme $ORIGINAL_COLOR_SCHEME
           gsettings set org.gnome.desktop.interface gtk-theme $ORIGINAL_GTK_THEME
           rm -rf ~/DropScreenshots"
    rm -rf "$TEMPORARY"
}

trap cleanup EXIT

set_guest_theme() {
    if [ "$1" = "Light" ]; then
        guest "gsettings set org.gnome.desktop.interface color-scheme default
               gsettings set org.gnome.desktop.interface gtk-theme Yaru"
    else
        guest "gsettings set org.gnome.desktop.interface color-scheme prefer-dark
               gsettings set org.gnome.desktop.interface gtk-theme Yaru-dark"
    fi

    sleep 1
}

# Saves the focused guest window to the given file
capture_window() {
    local output="$1"

    PREVIOUS=$(guest "ls -t ~/Pictures/Screenshots/ 2>/dev/null | head -1")

    # Alt+Print: GNOME saves the focused window with its frame and transparent shadow
    VBoxManage controlvm "$VM_NAME" keyboardputscancode 38 54 d4 b8

    for i in $(seq 10); do
        sleep 1
        SCREENSHOT=$(guest "ls -t ~/Pictures/Screenshots/ 2>/dev/null | head -1")
        [ "$SCREENSHOT" != "$PREVIOUS" ] && break
    done

    if [ "$SCREENSHOT" = "$PREVIOUS" ]; then
        echo "ERROR: $output was not taken."
        exit 1
    fi

    sleep 1 # Let GNOME finish writing the file

    scp "${SCP_OPTIONS[@]}" "$SSH_TARGET:Pictures/Screenshots/$SCREENSHOT" "$output"
    guest "rm -f ~/Pictures/Screenshots/'$SCREENSHOT'"

    echo "$output updated."
}

# Opens the link from standard input in headless Chrome as a phone, picks the given files, saves
# the page to the given file once about half of the files is sent, and closes Chrome. Chrome talks
# to it over the DevTools protocol, so nothing has to be installed.
cat > "$TEMPORARY/Page.mjs" <<'EOF'
import { spawn } from 'node:child_process'
import { readFileSync, writeFileSync } from 'node:fs'
import { setTimeout as sleep } from 'node:timers/promises'

const [chromePath, profile, output, ...files] = process.argv.slice(2)
let link = ''
for await (const part of process.stdin) link += part
const url = new URL(link.trim())

// The page has to see the address of the guest, Drop answers only to its own address
const chrome = spawn(chromePath, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`,
  '--no-first-run', '--lang=en-US', `--host-resolver-rules=MAP ${url.hostname} 127.0.0.1`, 'about:blank'],
  { stdio: 'ignore' })

try {
  let port
  for (let i = 0; i < 100 && !port; i++) {
    await sleep(100)
    try {
      port = readFileSync(`${profile}/DevToolsActivePort`, 'utf8').split('\n')[0]
    } catch {}
  }
  const targets = await fetch(`http://127.0.0.1:${port}/json`).then((response) => response.json())
  const socket = new WebSocket(targets.find((target) => target.type === 'page').webSocketDebuggerUrl)
  await new Promise((resolve) => socket.addEventListener('open', resolve))

  let id = 0
  const waiting = new Map()
  socket.addEventListener('message', (event) => {
    const message = JSON.parse(event.data)
    waiting.get(message.id)?.(message)
    waiting.delete(message.id)
  })
  const send = (method, params = {}) =>
    new Promise((resolve) => {
      waiting.set(++id, resolve)
      socket.send(JSON.stringify({ id, method, params }))
    })
  const evaluate = async (expression) =>
    (await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true })).result?.result?.value

  await send('Emulation.setDeviceMetricsOverride', { width: 390, height: 760, deviceScaleFactor: 2, mobile: true })
  await send('Emulation.setLocaleOverride', { locale: 'en-US' })
  await send('Page.navigate', { url: url.href })

  for (let i = 0; i < 100 && !(await evaluate('!!document.querySelector("input[type=file]")')); i++) {
    await sleep(100)
  }

  // Picked the way a person picks them, the change event included
  const { root } = (await send('DOM.getDocument')).result
  const { nodeId } = (await send('DOM.querySelector', { nodeId: root.nodeId, selector: 'input[type=file]' })).result
  await send('DOM.setFileInputFiles', { nodeId, files })

  // About half of the long video: "170 MB of 495 MB"
  for (let i = 0; i < 1200; i++) {
    const sent = await evaluate('Number(document.body.innerText.match(/([0-9.]+) MB of/)?.[1] ?? 0)')
    if (sent >= 170) break
    await sleep(100)
  }

  if (output) {
    const shot = await send('Page.captureScreenshot', { format: 'png' })
    writeFileSync(output, Buffer.from(shot.result.data, 'base64'))
    console.log(`${output} updated.`)
  }
} finally {
  chrome.kill()
}
EOF

# A drop in each theme, the page is the same in both

for THEME in Light Dark; do
    set_guest_theme "$THEME"

    guest 'rm -rf ~/DropScreenshots/Config/Mayfleet ~/DropScreenshots/Downloads/*
           LANGUAGE=en_US LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 QT_QPA_PLATFORM=xcb QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 \
               setsid ~/DropScreenshots/Package/opt/Drop/Drop </dev/null >/dev/null 2>&1 &
           w=$(xdotool search --sync --onlyvisible --class Drop | head -1)
           xdotool windowsize --sync $w 640 340
           xdotool windowactivate --sync $w 2>/dev/null || true
           xdotool mousemove 0 0' # Keep the pointer away from the window

    PORT=$(guest 'for i in $(seq 40); do
                      address=$(ss -ltnpH | grep "\"Drop\"" | grep -oE "[0-9.]+:[0-9]+" | head -1)
                      [ -n "$address" ] && break
                      sleep 0.5
                  done
                  echo ${address##*:}')

    if [ -z "$PORT" ]; then
        echo "ERROR: Drop did not start in the guest."
        exit 1
    fi

    # Copy Link through accessibility, then the link from the clipboard: it is never shown
    LINK=$(guest 'GDK_BACKEND=x11 python3 - <<EOF
import time
import gi
gi.require_version("Atspi", "2.0")
gi.require_version("Gdk", "3.0")
gi.require_version("Gtk", "3.0")
from gi.repository import Atspi, Gdk, Gtk

def find_button(node, name):
    if node.get_role_name() in ("button", "push button") and node.get_name() == name:
        return node
    for i in range(node.get_child_count()):
        button = find_button(node.get_child_at_index(i), name)
        if button:
            return button
    return None

desktop = Atspi.get_desktop(0)

for i in range(desktop.get_child_count()):
    application = desktop.get_child_at_index(i)
    if application.get_name() == "Drop":
        button = find_button(application, "Copy Link")
        if button:
            button.get_action_iface().do_action(0)
            break

time.sleep(1)
print(Gtk.Clipboard.get(Gdk.SELECTION_CLIPBOARD).wait_for_text() or "")
EOF')

    if [[ "$LINK" != http://*#* ]]; then
        echo "ERROR: The link was not copied in the guest."
        exit 1
    fi

    GUEST_ADDRESS=$(echo "$LINK" | sed -E 's|^http://([^:/]+):.*|\1|')

    ssh "${SSH_OPTIONS[@]}" -N -L "127.0.0.1:$PORT:$GUEST_ADDRESS:$PORT" "$SSH_TARGET" &
    TUNNEL_PID=$!

    for i in $(seq 40); do
        (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null && break
        sleep 0.25
    done

    PAGE=
    [ "$THEME" = "Light" ] && PAGE="Assets/Page.png"

    rm -rf "$TEMPORARY/Chrome"
    echo "$LINK" | node "$TEMPORARY/Page.mjs" "$CHROME" "$TEMPORARY/Chrome" "$PAGE" "${FILES[@]}"

    sleep 1 # Let the window draw the last chunks

    capture_window "Assets/Window-$THEME.png"

    stop_drop
done

# Keep image size in README in sync, so the page does not jump while screenshots load. The window
# is shown as it is, the phone narrow enough to stand beside the steps of How to Use.

SIZE=$(file -b "Assets/Window-Light.png" | grep -oE '[0-9]+ x [0-9]+')
WIDTH=${SIZE% x *}
HEIGHT=${SIZE#* x }

PAGE_SIZE=$(file -b "Assets/Page.png" | grep -oE '[0-9]+ x [0-9]+')
PAGE_WIDTH=200
PAGE_HEIGHT=$((${PAGE_SIZE#* x } * PAGE_WIDTH / ${PAGE_SIZE% x *}))

sed -i -E "s|(src=\"Assets/Window-Light.png\" width=\")[0-9]+(\" height=\")[0-9]+|\1$WIDTH\2$HEIGHT|" README.md
sed -i -E "s|(src=\"Assets/Page.png\" width=\")[0-9]+(\" height=\")[0-9]+|\1$PAGE_WIDTH\2$PAGE_HEIGHT|" README.md

echo "README.md: window size set to ${WIDTH}x${HEIGHT}, page size to ${PAGE_WIDTH}x${PAGE_HEIGHT}."
