<template>
  <q-layout view="hHh lpR fFf">
    <q-page-container>
      <q-page class="column no-wrap q-pa-md text-accent page">
        <div class="row items-center no-wrap q-mb-md">
          <img src="/favicon.svg" alt="" width="40" height="40" />
          <div class="text-h5 q-ml-sm">Drop</div>
        </div>

        <div v-if="!secret" class="col column flex-center text-center text-body1 text-warning">
          {{ t.linkIncomplete }}
        </div>

        <template v-else>
          <q-btn
            unelevated
            color="primary"
            text-color="ink"
            size="lg"
            icon="add_photo_alternate"
            :label="t.chooseFiles"
            :disable="!!state.stopped"
            @click="choose"
          />
          <input ref="input" type="file" multiple hidden @change="picked" />

          <q-banner v-if="state.stopped" rounded class="bg-negative text-white q-mt-md">
            {{ t.outOfSpace }}
          </q-banner>
          <q-banner v-else-if="statusError" rounded class="bg-negative text-white q-mt-md">
            {{ describeError(statusError) }}.
          </q-banner>
          <q-banner v-else-if="state.offline" rounded class="bg-warning text-white q-mt-md">
            <template #avatar><q-spinner size="24px" /></template>
            {{ t.connectionLost }}
          </q-banner>

          <div v-if="state.items.length > 0" class="q-mt-lg">
            <div class="row items-baseline justify-between text-body2">
              <div>{{ t.filesSent(state.doneCount, state.items.length) }}</div>
              <div class="text-caption">
                {{ t.sizeOf(size(state.sentSize), size(state.totalSize)) }}
              </div>
            </div>
            <q-linear-progress
              rounded
              size="8px"
              color="positive"
              track-color="grey-8"
              class="q-mt-xs"
              :value="state.totalSize > 0 ? state.sentSize / state.totalSize : 1"
            />
            <div v-if="state.busy" class="text-caption text-grey-5 q-mt-sm">
              {{ t.keepOpen }}
            </div>
          </div>

          <q-list v-if="state.items.length > 0" dense class="q-mt-md">
            <file-item
              v-for="(item, index) in state.items"
              :key="index"
              :name="item.name"
              :caption="item.status === 'failed' ? describeError(item.error) : size(item.size)"
              :icon="itemMarks[item.status]?.[0]"
              :icon-color="itemMarks[item.status]?.[1]"
              :progress="itemProgress(item)"
            />
          </q-list>

          <template v-if="visibleEarlier.length > 0">
            <div class="text-body2 q-mt-lg">{{ t.alreadyOnComputer }}</div>
            <q-list dense class="q-mt-sm">
              <file-item
                v-for="(file, index) in visibleEarlier"
                :key="index"
                :name="file.name"
                :caption="
                  file.complete ? size(file.size) : t.pickAgain(size(file.sent), size(file.size))
                "
                :icon="file.complete ? 'check_circle' : 'pause_circle'"
                :icon-color="file.complete ? 'positive' : 'grey-6'"
              />
            </q-list>
          </template>

          <div
            v-if="state.items.length === 0 && visibleEarlier.length === 0"
            class="col column flex-center text-center text-grey-5 q-pa-lg"
          >
            {{ t.emptyHint }}
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
import FileItem from '@/components/FileItem.vue'
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
  failed: ['error', 'negative'],
  waiting: ['schedule', 'grey-6']
}

function itemProgress(item) {
  if (item.status === 'preparing') return null
  if (item.status === 'uploading') return item.size > 0 ? item.sent / item.size : 0
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
/* White on the green of the icon is hard to read, its dark ink is not */
.text-ink {
  color: #0b1a0f !important;
}
</style>

<style scoped>
.page {
  max-width: 640px;
  margin: 0 auto;
}
</style>
