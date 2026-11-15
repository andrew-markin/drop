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
  noSleep ??= create()
  if (wanted && !playing()) {
    noSleep
      .enable()
      // Sending may be over by the time it starts
      .then(() => wanted || noSleep.disable())
      .catch(() => {}) // Not critical, the upload goes on anyway
  } else if (!wanted && noSleep.isEnabled) {
    noSleep.disable()
  }
}

// The system pauses the video when another app takes the sound, a notification for one, or when
// the page is hidden, and the video does not go on by itself. Started again at once, or on a return
// to the page or any tap there if that fails.
function create() {
  const created = new NoSleep()
  const video = created.noSleepVideo
  const resume = () => wanted && document.visibilityState === 'visible' && apply()
  video?.addEventListener('pause', resume)
  document.addEventListener('visibilitychange', resume)
  document.addEventListener('click', resume, true)
  if (video) show(video)
  return created
}

// iOS lets the screen sleep while a video plays out of the page, so it goes there, a pixel in a
// corner that is still visible
function show(video) {
  Object.assign(video.style, {
    position: 'fixed',
    left: '0',
    bottom: '0',
    width: '1px',
    height: '1px',
    pointerEvents: 'none'
  })
  video.setAttribute('aria-hidden', 'true')
  document.body.append(video)
}

function playing() {
  return noSleep.isEnabled && !noSleep.noSleepVideo?.paused
}
