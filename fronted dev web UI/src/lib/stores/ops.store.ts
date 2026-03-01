import { derived } from 'svelte/store'
import { runtimeStore } from './runtime.store'

export type StreamFilter = 'all' | 'status' | 'step_change' | 'error'

export const streamFilterOptions: Array<{ value: StreamFilter; label: string }> = [
  { value: 'all', label: 'Tous' },
  { value: 'status', label: 'Status' },
  { value: 'step_change', label: 'Transitions' },
  { value: 'error', label: 'Erreurs' },
]

export const createFilteredStream = (filter: () => StreamFilter) =>
  derived(runtimeStore, ($runtime) => {
    if (filter() === 'all') {
      return $runtime.streamEvents
    }
    return $runtime.streamEvents.filter((entry) => entry.raw.type === filter())
  })

export const opsStore = {
  refreshAudit: runtimeStore.refreshAudit,
}
