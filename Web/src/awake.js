import NoSleep from 'nosleep.js'

// Keeps the screen on while files are sent, a sleeping phone pauses the page. Over plain HTTP
// the Wake Lock API is not there, and NoSleep falls back to a tiny looping video. It has to be
// started from a tap, so enable() is called from the click handler that opens the file picker.
let noSleep

export function keepAwake(enabled) {
  noSleep ??= new NoSleep()
  if (enabled && !noSleep.isEnabled) {
    noSleep.enable().catch(() => {}) // Not critical, the upload goes on anyway
  } else if (!enabled && noSleep.isEnabled) {
    noSleep.disable()
  }
}
