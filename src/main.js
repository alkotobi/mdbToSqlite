import { createApp } from 'vue'
import './style.css'
import App from './App.vue'

function mountApp() {
  createApp(App).mount('#app')
}
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', mountApp)
} else {
  mountApp()
}
