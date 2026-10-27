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
            <q-item v-for="(item, index) in state.items" :key="index" class="q-px-none">
              <q-item-section>
                <q-item-label lines="1">{{ item.name }}</q-item-label>
                <q-item-label caption>
                  <template v-if="item.status === 'failed'">{{
                    describeError(item.error)
                  }}</template>
                  <template v-else>{{ size(item.size) }}</template>
                </q-item-label>
                <q-linear-progress
                  v-if="item.status === 'uploading' || item.status === 'preparing'"
                  rounded
                  size="4px"
                  color="primary"
                  track-color="grey-8"
                  class="q-mt-xs"
                  :indeterminate="item.status === 'preparing'"
                  :value="item.size > 0 ? item.sent / item.size : 0"
                />
              </q-item-section>
              <q-item-section side>
                <q-icon v-if="item.status === 'done'" name="check_circle" color="positive" />
                <q-icon v-else-if="item.status === 'failed'" name="error" color="negative" />
                <q-icon v-else-if="item.status === 'waiting'" name="schedule" color="grey-6" />
              </q-item-section>
            </q-item>
          </q-list>

          <template v-if="visibleEarlier.length > 0">
            <div class="text-body2 q-mt-lg">{{ t.alreadyOnComputer }}</div>
            <q-list dense class="q-mt-sm">
              <q-item v-for="(file, index) in visibleEarlier" :key="index" class="q-px-none">
                <q-item-section>
                  <q-item-label lines="1">{{ file.name }}</q-item-label>
                  <q-item-label caption>
                    <template v-if="file.complete">{{ size(file.size) }}</template>
                    <template v-else>
                      {{ t.pickAgain(size(file.sent), size(file.size)) }}
                    </template>
                  </q-item-label>
                </q-item-section>
                <q-item-section side>
                  <q-icon v-if="file.complete" name="check_circle" color="positive" />
                  <q-icon v-else name="pause_circle" color="grey-6" />
                </q-item-section>
              </q-item>
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

import { Api } from '@/api'
import { keepAwake } from '@/awake'
import { readEarlier, withoutPicked } from '@/earlier'
import { formatSize } from '@/format'
import { useI18n } from '@/i18n'
import { readSecret } from '@/link'
import { Uploader } from '@/uploader'

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
const visibleEarlier = computed(() => withoutPicked(earlier.value, state.value.items))
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
