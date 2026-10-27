import { computed, ref, watchEffect } from 'vue'

import messages from '@/locales'

// Picks the first of the user's preferred languages there is a translation for
export function resolveLocale(languages) {
  for (const tag of languages) {
    let locale
    try {
      locale = new Intl.Locale(tag)
    } catch {
      continue
    }
    if (locale.language in messages) return { tag: locale.toString(), key: locale.language }
  }
  return { tag: 'en', key: 'en' }
}

let i18n

export function useI18n() {
  if (i18n) return i18n

  const preferred = () => navigator.languages ?? [navigator.language]
  const current = ref(resolveLocale(preferred()))
  window.addEventListener('languagechange', () => {
    current.value = resolveLocale(preferred())
  })

  const locale = computed(() => current.value.tag)
  watchEffect(() => {
    document.documentElement.lang = locale.value
  })

  i18n = {
    locale,
    t: computed(() => ({ ...messages.en, ...messages[current.value.key] }))
  }
  return i18n
}
