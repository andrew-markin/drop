import { beforeEach, describe, expect, it, vi } from 'vitest'

// A NoSleep whose enable() fails until a tap allows it, like the video without a user gesture
const fake = vi.hoisted(() => ({ allowed: false, enables: 0, disables: 0 }))

vi.mock('nosleep.js', () => ({
  default: class {
    isEnabled = false
    enable() {
      fake.enables++
      if (!fake.allowed) return Promise.reject(new Error('Not allowed'))
      // Like NoSleep, enabled only once the video plays or the lock is taken
      return Promise.resolve().then(() => {
        this.isEnabled = true
      })
    }
    disable() {
      fake.disables++
      this.isEnabled = false
    }
  }
}))

// Lets the promise of enable() settle
const settle = () => new Promise((resolve) => setTimeout(resolve))

describe('keepAwake', () => {
  let awake

  beforeEach(async () => {
    Object.assign(fake, { allowed: false, enables: 0, disables: 0 })
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
})
