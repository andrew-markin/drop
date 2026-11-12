<template>
  <div class="file-card" :class="{ faded }">
    <div
      v-if="progress !== undefined || done"
      :key="mode"
      class="fill"
      :class="[`fill-${tone}`, { running: progress === null && !done, afterglow: done }]"
      :style="{ width: done || progress === null ? '100%' : `${progress * 100}%` }"
    />
    <div class="content row items-center no-wrap">
      <div class="col" style="min-width: 0">
        <div class="row no-wrap name">
          <span class="ellipsis base">{{ parts[0] }}</span>
          <span class="extension">{{ parts[1] }}</span>
        </div>
        <div class="caption" :class="{ 'text-negative': tone === 'error' }">{{ caption }}</div>
      </div>
      <q-icon v-if="icon" :name="icon" :color="iconColor" size="22px" class="q-ml-sm" />
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

// A file on a light card: its name, a line under it, and a mark of its state on the side. The card
// fills from the left with the progress as in the desktop application: a number fills that part,
// null runs a stripe across, and a file that is done keeps the full fill for a moment, fading away.
const props = defineProps({
  name: { type: String, required: true },
  caption: { type: String, default: '' },
  icon: { type: String, default: undefined },
  iconColor: { type: String, default: undefined },
  progress: { type: Number, default: undefined },
  // progress, error or paused
  tone: { type: String, default: 'progress' },
  done: { type: Boolean, default: false },
  faded: { type: Boolean, default: false }
})

// A fill of its own for each mode, so the full width of the running stripe does not shrink as a
// solid fill down to the progress of a file that has started
const mode = computed(() => (props.done ? 'done' : props.progress === null ? 'running' : 'filled'))

// The end of a name tells the type, so the name gives way before its extension does
const parts = computed(() => {
  const dot = props.name.lastIndexOf('.')
  return dot > 0 ? [props.name.slice(0, dot), props.name.slice(dot)] : [props.name, '']
})
</script>

<style scoped>
.file-card {
  position: relative;
  overflow: hidden;
  border-radius: 8px;
  background: #f7e6cc;
  color: #2f2a26;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.3);
}

/* Lighter text by its color, not by opacity, which would fade the selection too */
.faded .name {
  color: rgba(47, 42, 38, 0.5);
}

.content {
  position: relative;
  padding: 10px 12px;
}

.name {
  min-width: 0;
  font-size: 15px;
  line-height: 20px;
}

.base {
  min-width: 0;
}

.extension {
  flex-shrink: 0;
  white-space: pre;
}

.caption {
  font-size: 12px;
  line-height: 16px;
  color: rgba(47, 42, 38, 0.7);
}

.faded .caption {
  color: rgba(47, 42, 38, 0.35);
}

.fill {
  position: absolute;
  inset: 0 auto 0 0;
  transition: width 0.3s ease-out;
}

/* Over the cream, so the text stays readable over the filled part */
.fill-progress {
  background: rgba(63, 158, 77, 0.35);
}

.fill-error {
  background: rgba(222, 90, 58, 0.3);
}

.fill-paused {
  background: rgba(47, 42, 38, 0.12);
}

.running {
  background: linear-gradient(90deg, transparent 0%, rgba(63, 158, 77, 0.35) 50%, transparent 100%);
  background-size: 50% 100%;
  background-repeat: no-repeat;
  animation: running 1.2s linear infinite;
}

@keyframes running {
  from {
    background-position: -100% 0;
  }
  to {
    background-position: 200% 0;
  }
}

.afterglow {
  animation: afterglow 1.5s ease-in forwards;
}

@keyframes afterglow {
  0%,
  40% {
    opacity: 1;
  }
  100% {
    opacity: 0;
  }
}
</style>
