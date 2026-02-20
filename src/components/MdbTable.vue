<script setup>
const props = defineProps({
  selectedFolder: { type: String, default: '' },
  mdbFilesLength: { type: Number, default: 0 },
  filteredFiles: { type: Array, default: () => [] },
  filterText: { type: String, default: '' },
  titleFilter: { type: String, default: '' },
  authorFilter: { type: String, default: '' },
  categoryFilter: { type: String, default: '' },
  showSelectedOnly: { type: Boolean, default: false },
  allSelected: { type: Boolean, default: false },
  categories: { type: Array, default: () => [] },
  canLoadMore: { type: Boolean, default: false },
})

const emit = defineEmits([
  'update:filterText',
  'update:titleFilter',
  'update:authorFilter',
  'update:categoryFilter',
  'update:showSelectedOnly',
  'toggleSelectAll',
  'toggleRowSelection',
  'convertSingle',
  'loadMore',
])

const handleScroll = (event) => {
  if (!props.canLoadMore) {
    return
  }
  const el = event.target
  if (!el) return
  const threshold = 40
  if (el.scrollTop + el.clientHeight >= el.scrollHeight - threshold) {
    emit('loadMore')
  }
}
</script>

<template>
  <div class="folder-info">
    <p class="label">Selected Folder:</p>
    <p class="path">{{ selectedFolder }}</p>
    <div class="list-header">
      <p class="label">MDB Files Found: {{ mdbFilesLength }}</p>
      <div class="list-controls">
        <input
          :value="titleFilter"
          type="text"
          class="filter-input"
          placeholder="Filter title..."
          @input="emit('update:titleFilter', $event.target.value)"
        />
        <input
          :value="authorFilter"
          type="text"
          class="filter-input"
          placeholder="Filter author..."
          @input="emit('update:authorFilter', $event.target.value)"
        />
        <select
          :value="categoryFilter"
          class="filter-select"
          @change="emit('update:categoryFilter', $event.target.value)"
        >
          <option value="">All categories</option>
          <option v-for="cat in categories" :key="cat" :value="cat">
            {{ cat }}
          </option>
        </select>
        <label class="toggle-selected">
          <input
            type="checkbox"
            class="checkbox-large"
            :checked="showSelectedOnly"
            @change="emit('update:showSelectedOnly', $event.target.checked)"
          />
          <span>Only selected</span>
        </label>
        <input
          :value="filterText"
          type="text"
          class="filter-input"
          placeholder="Filter paths..."
          @input="emit('update:filterText', $event.target.value)"
        />
      </div>
    </div>

    <div
      v-if="filteredFiles.length"
      class="file-table-wrapper"
      @scroll="handleScroll"
    >
      <table class="file-table">
        <thead>
          <tr>
            <th class="col-select">
              <input
                type="checkbox"
                :checked="allSelected"
                @change="emit('toggleSelectAll')"
              />
            </th>
            <th class="col-book">Book</th>
            <th class="col-author">Author</th>
            <th class="col-category">Category</th>
            <th class="col-path">Path</th>
            <th class="col-actions">Actions</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="file in filteredFiles" :key="file.path">
            <td class="col-select">
              <input
                type="checkbox"
                :checked="file.selected"
                @change="emit('toggleRowSelection', file)"
              />
            </td>
            <td class="col-book">
              <span class="book-title">
                {{ file.bookName || 'Unknown book' }}
              </span>
            </td>
            <td class="col-author">
              <span v-if="file.authorName" class="author-line">
                {{ file.authorName }}
              </span>
            </td>
            <td class="col-category">
              <span v-if="file.categoryName" class="category-line">
                {{ file.categoryName }}
              </span>
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
                @click="emit('convertSingle', file)"
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
</template>

<style scoped>
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
  flex-wrap: wrap;
  justify-content: flex-end;
}

.toggle-selected {
  display: inline-flex;
  align-items: center;
  gap: 0.35rem;
  font-size: 0.85rem;
  color: #374151;
  white-space: nowrap;
}

.checkbox-large {
  width: 18px;
  height: 18px;
}

.filter-input {
  width: 200px;
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

.filter-select {
  max-width: 180px;
  padding: 0.4rem 0.6rem;
  border-radius: 0.375rem;
  border: 1px solid rgba(148, 163, 184, 0.7);
  background-color: #ffffff;
  color: #111827;
  font-size: 0.85rem;
}

.filter-select:focus {
  outline: none;
  border-color: #6366f1;
  box-shadow: 0 0 0 1px rgba(99, 102, 241, 0.7);
}

.file-table-wrapper {
  margin-top: 0.75rem;
  max-height: 320px;
  overflow-y: scroll;
  border-radius: 0.5rem;
  border: 1px solid rgba(209, 213, 219, 0.9);
  background-color: #ffffff;
  scrollbar-width: thin;
  scrollbar-color: #9ca3af #e5e7eb;
}

.file-table-wrapper::-webkit-scrollbar {
  width: 10px;
}

.file-table-wrapper::-webkit-scrollbar-track {
  background: #e5e7eb;
}

.file-table-wrapper::-webkit-scrollbar-thumb {
  background-color: #9ca3af;
  border-radius: 999px;
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

.book-title {
  font-size: 0.95rem;
  font-weight: 600;
  color: #111827;
}

.category-line {
  font-size: 0.8rem;
  color: #6b7280;
}

.author-line {
  font-size: 0.8rem;
  color: #374151;
}

.load-more {
  padding: 0.5rem;
  text-align: center;
  background-color: #f9fafb;
  border-top: 1px solid rgba(229, 231, 235, 1);
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
