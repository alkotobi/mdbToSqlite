import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import fs from 'fs'
import path from 'path'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

// Post-process built index.html for file://: strip module type, remove crossorigin, move script to end of body
function fixIndexForFileProtocol() {
  return {
    name: 'fix-index-for-file',
    closeBundle() {
      const htmlPath = path.resolve(__dirname, 'dist/index.html')
      if (!fs.existsSync(htmlPath)) return
      let html = fs.readFileSync(htmlPath, 'utf8')
      // Remove type="module" (blocked on file://)
      html = html.replace(/\s*type="module"\s*/g, ' ')
      // Remove crossorigin (can break script load on file://)
      html = html.replace(/\s*crossorigin\s*/g, ' ')
      // Move app script from head to end of body so #app exists when it runs
      const scriptMatch = html.match(/<script[^>]*src="([^"]+)"[^>]*>\s*<\/script>/)
      if (scriptMatch) {
        const scriptTag = scriptMatch[0]
        const src = scriptMatch[1]
        html = html.replace(scriptTag, '')
        html = html.replace('</body>', `  <script src="${src}"></script>\n</body>`)
      }
      fs.writeFileSync(htmlPath, html)
    },
  }
}

// https://vite.dev/config/
// base: './' so the built app works when opening index.html directly (e.g. from CrossDev file://)
// IIFE + single file so file:// doesn't hit ES module CORS restrictions
export default defineConfig({
  plugins: [vue(), fixIndexForFileProtocol()],
  base: './',
  build: {
    rollupOptions: {
      output: {
        format: 'iife',
        inlineDynamicImports: true,
      },
    },
  },
})
