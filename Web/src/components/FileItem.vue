<template>
  <q-item class="q-px-none">
    <q-item-section>
      <q-item-label lines="1">{{ name }}</q-item-label>
      <q-item-label caption>{{ caption }}</q-item-label>
      <q-linear-progress
        v-if="progress !== undefined"
        rounded
        size="4px"
        color="primary"
        track-color="grey-8"
        class="q-mt-xs"
        :indeterminate="progress === null"
        :value="progress ?? 0"
      />
    </q-item-section>
    <q-item-section side>
      <q-icon v-if="icon" :name="icon" :color="iconColor" />
    </q-item-section>
  </q-item>
</template>

<script setup>
// A file in a list: its name, a line under it, and a mark of its state on the side.
// The progress bar shows for a number, and runs without one for null.
defineProps({
  name: { type: String, required: true },
  caption: { type: String, default: '' },
  icon: { type: String, default: undefined },
  iconColor: { type: String, default: undefined },
  progress: { type: Number, default: undefined }
})
</script>
