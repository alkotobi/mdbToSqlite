<script setup>
import { computed } from 'vue'

const props = defineProps({
  isLoading: { type: Boolean, default: false },
  isBatchConverting: { type: Boolean, default: false },
  hasFiles: { type: Boolean, default: false },
  hasSelected: { type: Boolean, default: false },
})

const emit = defineEmits(['choose-folder', 'batch-convert'])

const canBatchConvert = computed(
  () => props.hasFiles && props.hasSelected && !props.isBatchConverting
)
</script>

<template>
  <div class="folder-selector">
    <button
      class="btn btn-primary"
      @click="emit('choose-folder')"
      :disabled="isLoading"
    >
      {{ isLoading ? 'Selecting...' : 'Choose Folder' }}
    </button>
    <button
      class="btn btn-secondary"
      @click="emit('batch-convert')"
      :disabled="!canBatchConvert"
    >
      {{ isBatchConverting ? 'Batch converting...' : 'Batch convert to folder...' }}
    </button>
  </div>
</template>

<style scoped>
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
</style>

