<script setup>
import { ref } from 'vue'

const selectedFolder = ref('')
const mdbFiles = ref([])
const isLoading = ref(false)
const statusMessage = ref('')
const isBatchConverting = ref(false)

const hasCrossDev = () => typeof window !== 'undefined' && window.CrossDev && typeof window.CrossDev.invoke === 'function'

const openFolderDialog = async () => {
  isLoading.value = true
  statusMessage.value = 'Opening folder selection...'
  mdbFiles.value = []

  try {
    if (hasCrossDev()) {
      const response = await window.CrossDev.invoke('mdbSelectFolder', {})
      if (!response || !response.success) {
        statusMessage.value = response && response.error ? response.error : 'Folder selection cancelled'
        isLoading.value = false
        return
      }
      selectedFolder.value = response.folder || ''
      mdbFiles.value = Array.isArray(response.files)
        ? response.files.map((p) => ({
            path: p,
            selected: false,
            converting: false,
            status: '',
            sqlitePath: '',
          }))
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

const allSelected = () => mdbFiles.value.length > 0 && mdbFiles.value.every((f) => f.selected)
const anySelected = () => mdbFiles.value.some((f) => f.selected)

const toggleSelectAll = () => {
  const target = !allSelected()
  mdbFiles.value = mdbFiles.value.map((f) => ({ ...f, selected: target }))
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
      <div class="folder-selector">
        <button 
          class="btn btn-primary" 
          @click="openFolderDialog"
          :disabled="isLoading"
        >
          {{ isLoading ? 'Selecting...' : 'Choose Folder' }}
        </button>
        <button
          class="btn btn-secondary"
          @click="batchConvert"
          :disabled="!mdbFiles.length || !anySelected() || isBatchConverting"
        >
          {{ isBatchConverting ? 'Batch converting...' : 'Batch convert to folder...' }}
        </button>
      </div>
      
      <div v-if="selectedFolder" class="folder-display">
        <div class="folder-info">
          <p class="label">Selected Folder:</p>
          <p class="path">{{ selectedFolder }}</p>
          <p class="label" style="margin-top: 0.75rem;">MDB Files Found: {{ mdbFiles.length }}</p>

          <table v-if="mdbFiles.length" class="file-table">
            <thead>
              <tr>
                <th class="col-select">
                  <input
                    type="checkbox"
                    :checked="allSelected()"
                    @change="toggleSelectAll"
                  />
                </th>
                <th class="col-path">Path</th>
                <th class="col-actions">Actions</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="file in mdbFiles" :key="file.path">
                <td class="col-select">
                  <input
                    type="checkbox"
                    :checked="file.selected"
                    @change="toggleRowSelection(file)"
                  />
                </td>
                <td class="col-path">
                  <div class="path">{{ file.path }}</div>
                  <div v-if="file.sqlitePath" class="sqlite-path">
                    → {{ file.sqlitePath }}
                  </div>
                </td>
                <td class="col-actions">
                  <button
                    class="btn btn-small"
                    @click="convertSingleHere(file)"
                    :disabled="file.converting"
                  >
                    {{ file.converting ? 'Converting...' : 'Convert here' }}
                  </button>
                  <div v-if="file.status" class="status-chip">
                    {{ file.status }}
                  </div>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
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
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
}

.header {
  padding: 2rem;
  color: white;
  text-align: center;
}

.header h1 {
  font-size: 2.5rem;
  margin-bottom: 0.5rem;
  font-weight: 700;
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
  justify-content: center;
  gap: 2rem;
  padding: 2rem;
}

.folder-selector {
  display: flex;
  gap: 1rem;
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
  background-color: #fff;
  color: #667eea;
}

.btn-primary:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 10px 20px rgba(0, 0, 0, 0.2);
}

.btn-secondary {
  background-color: rgba(255, 255, 255, 0.9);
  color: #333;
}

.btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.folder-display {
  width: 100%;
  max-width: 600px;
}

.folder-info {
  background: rgba(255, 255, 255, 0.95);
  padding: 1.5rem;
  border-radius: 0.5rem;
  box-shadow: 0 4px 15px rgba(0, 0, 0, 0.1);
}

.label {
  font-weight: 600;
  color: #333;
  margin-bottom: 0.5rem;
}

.path {
  font-family: 'Monaco', 'Courier New', monospace;
  color: #666;
  word-break: break-all;
  font-size: 0.9rem;
}

.status-message {
  color: white;
  padding: 1rem;
  background: rgba(0, 0, 0, 0.2);
  border-radius: 0.5rem;
  max-width: 600px;
  text-align: center;
}

.file-table {
  width: 100%;
  margin-top: 0.75rem;
  border-collapse: collapse;
  font-size: 0.9rem;
}

.file-table th,
.file-table td {
  padding: 0.5rem;
  border-bottom: 1px solid #eee;
  vertical-align: top;
}

.file-table thead {
  background-color: #f5f5f5;
}

.col-select {
  width: 2.5rem;
  text-align: center;
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
  color: #888;
}

.status-chip {
  margin-top: 0.25rem;
  padding: 0.15rem 0.4rem;
  border-radius: 999px;
  background-color: rgba(0, 0, 0, 0.05);
  font-size: 0.75rem;
  display: inline-block;
}
</style>
