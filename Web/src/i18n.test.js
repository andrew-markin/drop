import { describe, expect, it } from 'vitest'

import { resolveLocale } from '@/i18n'
import messages from '@/locales'

describe('resolveLocale', () => {
  it('picks the first preferred language with a translation', () => {
    expect(resolveLocale(['ru-RU', 'en-US'])).toEqual({ tag: 'ru-RU', key: 'ru' })
    expect(resolveLocale(['de-DE', 'ru'])).toEqual({ tag: 'ru', key: 'ru' })
    expect(resolveLocale(['en-GB'])).toEqual({ tag: 'en-GB', key: 'en' })
  })

  it('falls back to English', () => {
    expect(resolveLocale(['de-DE', 'fr'])).toEqual({ tag: 'en', key: 'en' })
    expect(resolveLocale(['not a tag'])).toEqual({ tag: 'en', key: 'en' })
    expect(resolveLocale([])).toEqual({ tag: 'en', key: 'en' })
  })
})

describe('messages', () => {
  it('are translated completely', () => {
    for (const [language, translation] of Object.entries(messages)) {
      expect(Object.keys(translation).sort(), language).toEqual(Object.keys(messages.en).sort())
    }
  })

  it('agree in number with the files count', () => {
    expect(messages.en.filesSent(1, 1)).toBe('1 of 1 file sent')
    expect(messages.en.filesSent(0, 3)).toBe('0 of 3 files sent')
    expect(messages.ru.filesSent(1, 1)).toBe('Отправлено 1 из 1 файла')
    expect(messages.ru.filesSent(2, 2)).toBe('Отправлено 2 из 2 файлов')
    expect(messages.ru.filesSent(3, 5)).toBe('Отправлено 3 из 5 файлов')
    expect(messages.ru.filesSent(1, 21)).toBe('Отправлено 1 из 21 файла')
  })
})
