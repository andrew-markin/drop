<template>
  <q-layout view="hHh lpR fFf">
    <q-page-container>
      <q-page class="flex flex-center column q-pa-lg text-center text-accent">
        <q-icon name="water_drop" size="96px" color="primary" />
        <div class="text-h4 q-mt-md">Drop</div>
        <div v-if="secret" class="text-body1 q-mt-sm">The link is valid.</div>
        <div v-else class="text-body1 q-mt-sm text-warning">
          The link is incomplete. Scan the QR code shown by Drop on the computer again.
        </div>
      </q-page>
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { readSecret } from '@/link'

const secret = readSecret(location.hash)
</script>
