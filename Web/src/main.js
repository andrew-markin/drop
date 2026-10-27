import '@quasar/extras/roboto-font/roboto-font.css'
import '@quasar/extras/material-icons/material-icons.css'
import 'quasar/src/css/index.sass'

import { Quasar } from 'quasar'
import { createApp } from 'vue'

import App from '@/App.vue'

const app = createApp(App)

app.use(Quasar, { plugins: {}, config: { dark: true } })

app.mount('#app')
