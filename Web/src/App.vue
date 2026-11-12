<template>
  <q-layout view="hHh lpR fFf">
    <q-page-container>
      <q-page class="column no-wrap q-pa-md page">
        <div v-if="!secret" class="col column flex-center text-center text-body1 text-warning">
          {{ t.linkIncomplete }}
        </div>

        <template v-else>
          <input ref="input" type="file" multiple hidden @change="picked" />

          <q-banner v-if="state.stopped" rounded class="bg-negative text-cream q-mb-md">
            {{ t.outOfSpace }}
          </q-banner>
          <q-banner v-else-if="statusError" rounded class="bg-negative text-cream q-mb-md">
            {{ describeError(statusError) }}.
          </q-banner>
          <q-banner v-else-if="state.offline" rounded class="bg-warning text-ink q-mb-md">
            <template #avatar><q-spinner size="24px" /></template>
            {{ t.connectionLost }}
          </q-banner>

          <template v-if="state.items.length > 0">
            <div class="row items-baseline justify-between text-body2 text-cream q-mb-sm">
              <div>{{ t.filesSent(state.doneCount, state.items.length) }}</div>
              <div class="text-caption">
                {{ t.sizeOf(size(state.sentSize), size(state.totalSize)) }}
              </div>
            </div>
            <div v-if="state.busy" class="text-caption text-cream-muted q-mb-sm">
              {{ t.keepOpen }}
            </div>
            <div class="column no-wrap cards">
              <file-card
                v-for="(item, index) in state.items"
                :key="index"
                :name="item.name"
                :caption="item.status === 'failed' ? describeError(item.error) : size(item.size)"
                :icon="itemMarks[item.status]?.[0]"
                :icon-color="itemMarks[item.status]?.[1]"
                :progress="itemProgress(item)"
                :tone="item.status === 'failed' ? 'error' : 'progress'"
                :done="item.status === 'done'"
                :faded="item.status === 'waiting'"
              />
            </div>
          </template>

          <template v-if="visibleEarlier.length > 0">
            <div class="text-body2 text-cream q-mb-sm" :class="{ 'q-mt-lg': state.items.length }">
              {{ t.alreadyOnComputer }}
            </div>
            <div class="column no-wrap cards">
              <file-card
                v-for="(file, index) in visibleEarlier"
                :key="index"
                :name="file.name"
                :caption="
                  file.complete ? size(file.size) : t.pickAgain(size(file.sent), size(file.size))
                "
                :icon="file.complete ? 'check_circle' : 'pause_circle'"
                :icon-color="file.complete ? 'positive' : 'grey-7'"
                :progress="file.complete || file.size === 0 ? undefined : file.sent / file.size"
                tone="paused"
              />
            </div>
          </template>

          <div
            v-if="state.items.length === 0 && visibleEarlier.length === 0"
            class="col column flex-center text-center text-cream-muted q-pa-lg"
          >
            {{ t.emptyHint }}
          </div>

          <!-- Fixed at the bottom right of the page column, not of the window -->
          <div class="fab-rail">
            <q-btn
              fab
              color="primary"
              padding="8px"
              :title="t.chooseFiles"
              :aria-label="t.chooseFiles"
              :disable="!!state.stopped"
              @click="choose"
            >
              <!-- The drop of the page icon, public/favicon.svg -->
              <svg viewBox="0 0 64 64" width="40" height="40" aria-hidden="true">
                <path
                  d="M32.00 10.80L44.80 24.05A17.79 17.79 0 1 1 19.20 24.05ZM44.59 39.77A13.02 13.02 0 0 1 36.45 48.64A2.00 2.00 0 0 1 35.09 44.90A9.03 9.03 0 0 0 40.72 38.74A2.00 2.00 0 0 1 44.59 39.77Z"
                  fill="#f7e6cc"
                  fill-rule="evenodd"
                />
              </svg>
            </q-btn>
          </div>
        </template>
      </q-page>
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, shallowRef, useTemplateRef } from 'vue'

import { Api, readEarlier } from '@/api'
import { keepAwake } from '@/awake'
import FileCard from '@/components/FileCard.vue'
import { readSecret } from '@/crypto'
import { useI18n } from '@/i18n'
import { Uploader } from '@/uploader'
import { formatSize } from '@/utils'

const { locale, t } = useI18n()
const size = (value) => formatSize(value, locale.value)

const secret = readSecret(location.hash)
const input = useTemplateRef('input')

const snapshot = (uploader) => ({
  items: uploader.items.map(({ name, size, sent, status, error }) => ({
    name,
    size,
    sent,
    status,
    error
  })),
  doneCount: uploader.items.filter((item) => item.status === 'done').length,
  sentSize: uploader.sentSize,
  totalSize: uploader.totalSize,
  busy: uploader.busy,
  offline: uploader.offline,
  stopped: uploader.stopped
})

const api = new Api(secret)
const uploader = new Uploader(api, {
  onChange: (uploader) => {
    state.value = snapshot(uploader)
    keepAwake(uploader.busy)
  }
})

const state = shallowRef(snapshot(uploader))

const earlier = shallowRef([])
// Picked again on this page, they show with the files being sent instead
const visibleEarlier = computed(() =>
  earlier.value.filter(
    (file) => !state.value.items.some((item) => item.name === file.name && item.size === file.size)
  )
)
const statusError = shallowRef()

// After a reload the page shows what already reached the computer, and a wrong link shows at once
async function loadEarlier() {
  try {
    earlier.value = readEarlier(await api.status())
  } catch (error) {
    if (!error.retryable) statusError.value = error
  }
}

function choose() {
  keepAwake(true) // Needs the tap, see awake.js
  input.value.click()
}

function picked(event) {
  const files = [...event.target.files]
  event.target.value = '' // The same files can be picked again
  if (files.length > 0) uploader.add(files)
  else if (!uploader.busy) keepAwake(false)
}

const itemMarks = {
  done: ['check_circle', 'positive'],
  failed: ['error', 'negative']
}

function itemProgress(item) {
  if (item.status === 'preparing') return null
  if (item.status === 'uploading' || item.status === 'failed') {
    return item.size > 0 ? item.sent / item.size : 0
  }
  return undefined
}

function describeError(error) {
  if (error?.status === 507) return t.value.errorNoSpace
  if (error?.status === 401) return t.value.errorWrongLink
  if (error?.status === 404) return t.value.errorRestarted
  return t.value.errorOther
}

// Leaving the page in the middle of an upload loses the picked files
function warnBeforeLeaving(event) {
  if (uploader.busy) event.preventDefault()
}

onMounted(() => {
  window.addEventListener('beforeunload', warnBeforeLeaving)
  if (secret) loadEarlier()
})
onBeforeUnmount(() => window.removeEventListener('beforeunload', warnBeforeLeaving))
</script>

<style>
/* Text in the cream and the dark ink of the icon. Lighter text by its color, not by opacity,
   which would fade the selection too */
.text-cream {
  color: #f7e6cc !important;
}

.text-cream-muted {
  color: rgba(247, 230, 204, 0.7) !important;
}

.text-ink {
  color: #2f2a26 !important;
}
</style>

<style scoped>
.page {
  max-width: 640px;
  margin: 0 auto;
  /* The last card scrolls out from under the button */
  padding-bottom: 88px;
}

.fab-rail {
  position: fixed;
  right: 0;
  bottom: 16px;
  left: 0;
  max-width: 640px;
  margin: 0 auto;
  padding: 0 16px;
  display: flex;
  justify-content: flex-end;
  pointer-events: none;
}

.fab-rail > * {
  pointer-events: auto;
}

.cards {
  gap: 8px;
}
</style>
