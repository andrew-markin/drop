import { beforeEach, describe, expect, it, vi } from 'vitest'

// A NoSleep whose enable() fails until a tap allows it, like the video without a user gesture
const fake = vi.hoisted(() => ({ allowed: false, enables: 0, disables: 0 }))

vi.mock('nosleep.js', () => ({
  default: class {
    isEnabled = false
    noSleepVideo = Object.assign(new EventTarget(), { paused: true })
    constructor() {
      fake.video = this.noSleepVideo
    }
    enable() {
      fake.enables++
      if (!fake.allowed) return Promise.reject(new Error('Not allowed'))
      // Like NoSleep, enabled only once the video plays or the lock is taken
      return Promise.resolve().then(() => {
        this.isEnabled = true
        this.noSleepVideo.paused = false
      })
    }
    disable() {
      fake.disables++
      this.isEnabled = false
      this.noSleepVideo.paused = true
    }
  }
}))

// Paused by the system, not by the page
function pauseVideo() {
  fake.video.paused = true
  fake.video.dispatchEvent(new Event('pause'))
}

// Lets the promise of enable() settle
const settle = () => new Promise((resolve) => setTimeout(resolve))

describe('keepAwake', () => {
  let awake

  beforeEach(async () => {
    Object.assign(fake, { allowed: false, enables: 0, disables: 0 })
    vi.stubGlobal('document', Object.assign(new EventTarget(), { visibilityState: 'visible' }))
    vi.resetModules()
    awake = await import('@/awake')
  })

  it('does not try again on every change after a failed attempt', async () => {
    for (let i = 0; i < 5; i++) {
      awake.keepAwake(true)
      await settle()
    }
    expect(fake.enables).toBe(1)
  })

  it('tries again on a tap', async () => {
    awake.keepAwake(true)
    await settle()
    fake.allowed = true
    awake.keepAwakeOnTap()
    await settle()
    awake.keepAwake(true)
    expect(fake.enables).toBe(2)
  })

  it('lets the screen sleep once sending stops', async () => {
    fake.allowed = true
    awake.keepAwakeOnTap()
    await settle()
    awake.keepAwake(false)
    expect(fake.disables).toBe(1)
  })

  it('lets the screen sleep when sending stops before the lock is taken', async () => {
    fake.allowed = true
    awake.keepAwakeOnTap()
    awake.keepAwake(false)
    await settle()
    expect(fake.disables).toBe(1)
  })

  it('plays the video again when the system pauses it', async () => {
    fake.allowed = true
    awake.keepAwakeOnTap()
    await settle()
    pauseVideo()
    await settle()
    expect(fake.enables).toBe(2)
    expect(fake.video.paused).toBe(false)
  })

  it('plays the video again on any tap when it could not right away', async () => {
    fake.allowed = true
    awake.keepAwakeOnTap()
    await settle()
    fake.allowed = false
    pauseVideo()
    await settle()
    fake.allowed = true
    document.dispatchEvent(new Event('click'))
    await settle()
    expect(fake.enables).toBe(3)
    expect(fake.video.paused).toBe(false)
  })

  it('waits for the page to show again to play the video', async () => {
    fake.allowed = true
    awake.keepAwakeOnTap()
    await settle()
    document.visibilityState = 'hidden'
    pauseVideo()
    await settle()
    expect(fake.enables).toBe(1)
    document.visibilityState = 'visible'
    document.dispatchEvent(new Event('visibilitychange'))
    await settle()
    expect(fake.enables).toBe(2)
  })

  it('leaves the video paused once sending stops', async () => {
    fake.allowed = true
    awake.keepAwakeOnTap()
    await settle()
    awake.keepAwake(false)
    fake.video.dispatchEvent(new Event('pause'))
    document.dispatchEvent(new Event('click'))
    await settle()
    expect(fake.enables).toBe(1)
  })
})
