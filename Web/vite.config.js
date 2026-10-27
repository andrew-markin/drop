/* eslint-disable import-x/no-nodejs-modules */
import process from 'node:process'
import { fileURLToPath, URL } from 'node:url'

import { quasar, transformAssetUrls } from '@quasar/vite-plugin'
import vue from '@vitejs/plugin-vue'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  server: {
    port: 8086,
    strictPort: true,
    // Talk to a running desktop application during development:
    // DROP_SERVER=192.168.0.5:54246 npm run dev
    proxy: process.env.DROP_SERVER
      ? { '/api': { target: `http://${process.env.DROP_SERVER}`, changeOrigin: true } }
      : undefined
  },
  // The desktop application serves the page from its resources at the site root
  base: '/',
  build: {
    // Fewer files to embed into the application resources
    assetsInlineLimit: 8192
  },
  plugins: [
    vue({
      template: { transformAssetUrls }
    }),
    // @quasar/plugin-vite options list:
    // https://github.com/quasarframework/quasar/blob/dev/vite-plugin/index.d.ts
    quasar({
      sassVariables: fileURLToPath(new URL('./src/quasar.sass', import.meta.url))
    })
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url))
    }
  }
})
