import NoSleep from 'nosleep.js'

// Keeps the screen on while files are sent, a sleeping phone pauses the page. Over plain HTTP
// the Wake Lock API is not there, and NoSleep falls back to a tiny looping video. It has to be
// started from a tap, so keepAwakeOnTap() is called from the click handler that opens the file
// picker. Changes of the uploads call keepAwake() all the time: it acts only when sending starts
// or stops, so an attempt that failed without a tap is not repeated on every bit of progress.
let noSleep
let wanted = false

export function keepAwake(enabled) {
  if (enabled === wanted) return
  wanted = enabled
  apply()
}

export function keepAwakeOnTap() {
  wanted = true
  apply()
}

function apply() {
  noSleep ??= new NoSleep()
  if (wanted && !noSleep.isEnabled) {
    noSleep
      .enable()
      // Sending may be over by the time it starts
      .then(() => wanted || noSleep.disable())
      .catch(() => {}) // Not critical, the upload goes on anyway
  } else if (!wanted && noSleep.isEnabled) {
    noSleep.disable()
  }
}
