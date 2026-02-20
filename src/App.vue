<script setup>
import { ref, computed, watch } from 'vue'
import FolderToolbar from './components/FolderToolbar.vue'
import MdbTable from './components/MdbTable.vue'

const selectedFolder = ref('')
const mdbFiles = ref([])
const isLoading = ref(false)
const statusMessage = ref('')
const isBatchConverting = ref(false)
const filterText = ref('')
const titleFilter = ref('')
const authorFilter = ref('')
const categoryFilter = ref('')
const showSelectedOnly = ref(false)

const normalizeText = (value) => {
  if (!value) return ''
  let s = value.toString().normalize('NFD')
  s = s.replace(/[\u064B-\u065F\u0617-\u061A\u06D6-\u06ED\u0670\u0640]/g, '')
  return s.toLowerCase()
}

const hasCrossDev = () => typeof window !== 'undefined' && window.CrossDev && typeof window.CrossDev.invoke === 'function'

const openFolderDialog = async () => {
  isLoading.value = true
  statusMessage.value = 'Opening folder selection...'
  mdbFiles.value = []
  filterText.value = ''

  try {
    if (hasCrossDev()) {
      const response = await window.CrossDev.invoke('mdbSelectFolder', {})
      if (!response || !response.success) {
        statusMessage.value = response && response.error ? response.error : 'Folder selection cancelled'
        isLoading.value = false
        return
      }
      selectedFolder.value = response.folder || ''
      console.log('mdbSelectFolder debug:', {
        debug_mainPath: response.debug_mainPath,
        debug_bookCount: response.debug_bookCount,
        sampleFile: response.files && response.files[0],
      })
      mdbFiles.value = Array.isArray(response.files)
        ? response.files.map((item) => {
            if (typeof item === 'string') {
              return {
                path: item,
                bookName: '',
                categoryName: '',
                authorName: '',
                selected: false,
                converting: false,
                status: '',
                sqlitePath: '',
              }
            }
            const path = item && typeof item.path === 'string' ? item.path : ''
            return {
              path,
              bookName: item && typeof item.bookName === 'string' ? item.bookName : '',
              categoryName:
                item && typeof item.categoryName === 'string' ? item.categoryName : '',
              authorName:
                item && typeof item.authorName === 'string' ? item.authorName : '',
              selected: false,
              converting: false,
              status: '',
              sqlitePath: '',
            }
          })
        : []
      statusMessage.value = `Found ${mdbFiles.value.length} MDB files`
      isLoading.value = false
      return
    }
    statusMessage.value = 'CrossDev not available (running in browser)'
    isLoading.value = false
  } catch (error) {
    statusMessage.value = `Error: ${error && error.message ? error.message : String(error)}`
    isLoading.value = false
  }
}

const filteredFiles = computed(() => {
  let files = mdbFiles.value

  if (categoryFilter.value) {
    files = files.filter((f) => (f.categoryName || '') === categoryFilter.value)
  }

  const title = normalizeText(titleFilter.value.trim())
  if (title) {
    files = files.filter((f) => normalizeText(f.bookName || '').includes(title))
  }

  const author = normalizeText(authorFilter.value.trim())
  if (author) {
    files = files.filter((f) =>
      normalizeText(f.authorName || '').includes(author),
    )
  }

  const text = normalizeText(filterText.value.trim())
  if (text) {
    files = files.filter((f) => normalizeText(f.path || '').includes(text))
  }

  if (showSelectedOnly.value) {
    files = files.filter((f) => f.selected)
  }

  return files
})

const pageSize = 50
const visibleCount = ref(pageSize)

watch(filteredFiles, () => {
  visibleCount.value = pageSize
})

const visibleFiles = computed(() =>
  filteredFiles.value.slice(0, visibleCount.value),
)

const canLoadMore = computed(
  () => visibleCount.value < filteredFiles.value.length,
)

const loadMore = () => {
  if (visibleCount.value < filteredFiles.value.length) {
    visibleCount.value += pageSize
  }
}

const categories = computed(() => {
  const set = new Set()
  for (const f of mdbFiles.value) {
    if (f.categoryName) {
      set.add(f.categoryName)
    }
  }
  return Array.from(set).sort()
})

const allSelected = () =>
  filteredFiles.value.length > 0 && filteredFiles.value.every((f) => f.selected)
const anySelected = () => mdbFiles.value.some((f) => f.selected)

const toggleSelectAll = () => {
  const target = !allSelected()
  const visiblePaths = new Set(filteredFiles.value.map((f) => f.path))
  mdbFiles.value = mdbFiles.value.map((f) =>
    visiblePaths.has(f.path) ? { ...f, selected: target } : f
  )
}

const toggleRowSelection = (file) => {
  mdbFiles.value = mdbFiles.value.map((f) =>
    f.path === file.path ? { ...f, selected: !f.selected } : f
  )
}

const convertSingleHere = async (file) => {
  if (!hasCrossDev()) {
    statusMessage.value = 'CrossDev not available'
    return
  }
  mdbFiles.value = mdbFiles.value.map((f) =>
    f.path === file.path ? { ...f, converting: true, status: 'Converting...' } : f
  )
  try {
    const payload = { files: [file.path], outputDir: '' }
    const result = await window.CrossDev.invoke('mdbConvert', payload)
    let status = 'Done'
    let sqlitePath = ''
    if (result && Array.isArray(result.items) && result.items.length > 0) {
      const item = result.items[0]
      sqlitePath = item.sqlite || ''
      if (item.exitCode !== 0) {
        status = `Error (code ${item.exitCode})`
      }
    } else if (!result || !result.success) {
      status = 'Error'
    }
    mdbFiles.value = mdbFiles.value.map((f) =>
      f.path === file.path
        ? { ...f, converting: false, status, sqlitePath }
        : f
    )
  } catch (error) {
    mdbFiles.value = mdbFiles.value.map((f) =>
      f.path === file.path
        ? {
            ...f,
            converting: false,
            status: error && error.message ? error.message : 'Error',
          }
        : f
    )
  }
}

const chooseOutputFolder = async () => {
  if (!hasCrossDev()) {
    statusMessage.value = 'CrossDev not available'
    return null
  }
  const response = await window.CrossDev.invoke('mdbSelectFolder', {})
  if (!response || !response.success || !response.folder) {
    statusMessage.value =
      response && response.error ? response.error : 'Destination folder selection cancelled'
    return null
  }
  return response.folder
}

const batchConvert = async () => {
  if (!hasCrossDev()) {
    statusMessage.value = 'CrossDev not available'
    return
  }
  const selected = mdbFiles.value.filter((f) => f.selected)
  if (!selected.length) {
    statusMessage.value = 'No files selected for batch convert'
    return
  }
  const outputDir = await chooseOutputFolder()
  if (!outputDir) {
    return
  }
  isBatchConverting.value = true
  statusMessage.value = `Batch converting ${selected.length} file(s) to: ${outputDir}`
  try {
    const payload = {
      files: selected.map((f) => f.path),
      outputDir,
    }
    const result = await window.CrossDev.invoke('mdbConvert', payload)
    if (result && Array.isArray(result.items)) {
      const itemsByPath = {}
      for (const item of result.items) {
        if (item.mdb) itemsByPath[item.mdb] = item
      }
      mdbFiles.value = mdbFiles.value.map((f) => {
        const item = itemsByPath[f.path]
        if (!item) return f
        const status = item.exitCode === 0 ? 'Done' : `Error (code ${item.exitCode})`
        return {
          ...f,
          status,
          sqlitePath: item.sqlite || f.sqlitePath,
        }
      })
      const failed = result.items.filter((i) => i.exitCode !== 0).length
      if (failed > 0) {
        statusMessage.value = `Batch convert finished with ${failed} error(s)`
      } else {
        statusMessage.value = 'Batch convert finished successfully'
      }
    } else {
      statusMessage.value = 'Batch convert failed'
    }
  } catch (error) {
    statusMessage.value = `Batch convert error: ${
      error && error.message ? error.message : String(error)
    }`
  } finally {
    isBatchConverting.value = false
  }
}
</script>

<template>
  <div class="app-container">
    <header class="header">
      <h1>Convert Access to SQLite</h1>
      <p class="subtitle">Select an Access database folder to convert</p>
    </header>
    
    <main class="main-content">
      <FolderToolbar
        :is-loading="isLoading"
        :is-batch-converting="isBatchConverting"
        :has-files="!!mdbFiles.length"
        :has-selected="anySelected()"
        @choose-folder="openFolderDialog"
        @batch-convert="batchConvert"
      />

      <div v-if="selectedFolder" class="folder-display">
        <MdbTable
          :selected-folder="selectedFolder"
          :mdb-files-length="mdbFiles.length"
          :filtered-files="visibleFiles"
          v-model:filterText="filterText"
          v-model:titleFilter="titleFilter"
          v-model:authorFilter="authorFilter"
          v-model:categoryFilter="categoryFilter"
          v-model:showSelectedOnly="showSelectedOnly"
          :all-selected="allSelected()"
          :categories="categories"
          :can-load-more="canLoadMore"
          @toggle-select-all="toggleSelectAll"
          @toggle-row-selection="toggleRowSelection"
          @convert-single="convertSingleHere"
          @load-more="loadMore"
        />
      </div>

      <div v-if="statusMessage" class="status-message">
        {{ statusMessage }}
      </div>
    </main>
  </div>
</template>

<style scoped>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

.app-container {
  width: 100%;
  height: 100vh;
  display: flex;
  flex-direction: column;
  background: radial-gradient(circle at top, #e5edff 0%, #c7d2fe 35%, #e5e7eb 100%);
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
}

.header {
  padding: 1.75rem 2rem;
  color: #111827;
  text-align: center;
}

.header h1 {
  font-size: 2.7rem;
  margin-bottom: 0.5rem;
  font-weight: 700;
  letter-spacing: 0.03em;
}

.subtitle {
  font-size: 1.1rem;
  opacity: 0.9;
}

.main-content {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
  gap: 2rem;
  padding: 2.5rem;
}

.folder-selector {
  display: flex;
  gap: 1rem;
  flex-wrap: wrap;
}

.btn {
  padding: 0.75rem 1.5rem;
  border: none;
  border-radius: 0.5rem;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.3s ease;
}

.btn-primary {
  background: linear-gradient(135deg, #5c7cfa 0%, #4e5cf5 50%, #9164ff 100%);
  color: #f5f7ff;
  box-shadow: 0 10px 25px rgba(37, 63, 172, 0.4);
}

.btn-primary:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 16px 35px rgba(37, 63, 172, 0.55);
}

.btn-secondary {
  background-color: rgba(15, 23, 42, 0.9);
  color: #e5e7f5;
  border: 1px solid rgba(148, 163, 184, 0.5);
}

.btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.folder-display {
  width: 100%;
  max-width: 960px;
}

.folder-info {
  background: #f9fafb;
  padding: 1.5rem;
  border-radius: 0.5rem;
  box-shadow: 0 18px 45px rgba(148, 163, 184, 0.5);
  border: 1px solid rgba(148, 163, 184, 0.5);
}

.label {
  font-weight: 600;
  color: #111827;
  margin-bottom: 0.5rem;
}

.path {
  font-family: 'Monaco', 'Courier New', monospace;
  color: #374151;
  word-break: break-all;
  font-size: 0.9rem;
}

.list-header {
  margin-top: 0.75rem;
  display: flex;
  gap: 0.75rem;
  align-items: center;
  justify-content: space-between;
}

.list-controls {
  display: flex;
  gap: 0.75rem;
  align-items: center;
}

.toggle-selected {
  display: inline-flex;
  align-items: center;
  gap: 0.35rem;
  font-size: 0.85rem;
  color: #374151;
  white-space: nowrap;
}

.toggle-selected .checkbox-large {
  width: 18px;
  height: 18px;
}

.filter-input {
  flex: 0 0 260px;
  max-width: 100%;
  padding: 0.4rem 0.6rem;
  border-radius: 0.375rem;
  border: 1px solid rgba(148, 163, 184, 0.7);
  background-color: #ffffff;
  color: #111827;
  font-size: 0.85rem;
}

.filter-input::placeholder {
  color: #9ca3af;
}

.filter-input:focus {
  outline: none;
  border-color: #6366f1;
  box-shadow: 0 0 0 1px rgba(99, 102, 241, 0.7);
}

.status-message {
  color: #111827;
  padding: 1rem;
  background: rgba(255, 255, 255, 0.9);
  border-radius: 0.5rem;
  max-width: 960px;
  text-align: center;
}

.file-table-wrapper {
  margin-top: 0.75rem;
  max-height: 60vh;
  overflow-y: auto;
  border-radius: 0.5rem;
  border: 1px solid rgba(209, 213, 219, 0.9);
  background-color: #ffffff;
}

.file-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.9rem;
}

.file-table th,
.file-table td {
  padding: 0.5rem;
  border-bottom: 1px solid rgba(229, 231, 235, 1);
  vertical-align: top;
}

.file-table thead {
  background-color: #eff6ff;
  color: #1f2937;
}

.col-select {
  width: 3rem;
  text-align: center;
}

.col-select input[type='checkbox'] {
  width: 18px;
  height: 18px;
}

.col-actions {
  width: 10rem;
}

.btn-small {
  padding: 0.25rem 0.5rem;
  font-size: 0.8rem;
}

.sqlite-path {
  margin-top: 0.25rem;
  font-size: 0.8rem;
  color: #4b5563;
}

.status-chip {
  margin-top: 0.25rem;
  padding: 0.15rem 0.4rem;
  border-radius: 999px;
  background-color: rgba(59, 130, 246, 0.09);
  border: 1px solid rgba(59, 130, 246, 0.4);
  font-size: 0.75rem;
  display: inline-block;
}
</style>
