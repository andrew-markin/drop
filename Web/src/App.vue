<template>
  <q-layout view="hHh lpR fFf">
    <q-page-container>
      <q-page class="column no-wrap q-pa-md text-accent page">
        <div class="row items-center no-wrap q-mb-md">
          <q-icon name="water_drop" size="40px" color="primary" />
          <div class="text-h5 q-ml-sm">Drop</div>
        </div>

        <div v-if="!secret" class="col column flex-center text-center text-body1 text-warning">
          The link is incomplete. Scan the QR code shown by Drop on the computer again.
        </div>

        <template v-else>
          <q-btn
            unelevated
            color="primary"
            size="lg"
            icon="add_photo_alternate"
            label="Choose Files"
            :disable="!!state.stopped"
            @click="choose"
          />
          <input ref="input" type="file" multiple hidden @change="picked" />

          <q-banner v-if="state.stopped" rounded class="bg-negative text-white q-mt-md">
            The computer is out of disk space. Free some space and reload this page.
          </q-banner>
          <q-banner v-else-if="statusError" rounded class="bg-negative text-white q-mt-md">
            {{ describeError(statusError) }}.
          </q-banner>
          <q-banner v-else-if="state.offline" rounded class="bg-warning text-white q-mt-md">
            <template #avatar><q-spinner size="24px" /></template>
            The connection is lost, trying again. Check that this device is on the same network as
            the computer.
          </q-banner>

          <div v-if="state.items.length > 0" class="q-mt-lg">
            <div class="row items-baseline justify-between text-body2">
              <div>{{ state.doneCount }} of {{ state.items.length }} files sent</div>
              <div class="text-caption">
                {{ formatSize(state.sentSize) }} of {{ formatSize(state.totalSize) }}
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
              Keep this page open until all files are sent.
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
                  <template v-else>{{ formatSize(item.size) }}</template>
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
            <div class="text-body2 q-mt-lg">Already on the computer</div>
            <q-list dense class="q-mt-sm">
              <q-item v-for="(file, index) in visibleEarlier" :key="index" class="q-px-none">
                <q-item-section>
                  <q-item-label lines="1">{{ file.name }}</q-item-label>
                  <q-item-label caption>
                    <template v-if="file.complete">{{ formatSize(file.size) }}</template>
                    <template v-else>
                      {{ formatSize(file.sent) }} of {{ formatSize(file.size) }}, pick it again to
                      continue
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
            Photos, videos and any other files go to the Downloads folder of the computer.
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
import { readSecret } from '@/link'
import { Uploader } from '@/uploader'

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
  if (error?.status === 507) return 'The computer is out of disk space'
  if (error?.status === 401) return 'The link is wrong, scan the QR code again'
  if (error?.status === 404) return 'Drop on the computer was restarted, scan the QR code again'
  return 'Not sent, something went wrong'
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

<style scoped>
.page {
  max-width: 640px;
  margin: 0 auto;
}
</style>
