import { get, writable } from 'svelte/store'
import {
  getMediaFiles,
  playMedia,
  startRecord,
  stopMedia,
  stopRecord,
  type MediaKind,
} from '../runtimeService'
import { runtimeStore } from './runtime.store'

type MediaFilesMap = Record<MediaKind, string[]>

type MediaStoreState = {
  files: MediaFilesMap
  loading: Record<MediaKind, boolean>
  actionError: string
}

const EMPTY_FILES: MediaFilesMap = {
  music: [],
  picture: [],
  recorder: [],
}

const toErrorMessage = (error: unknown) => (error instanceof Error ? error.message : 'Erreur inconnue')

const createMediaStore = () => {
  const { subscribe, update, set } = writable<MediaStoreState>({
    files: EMPTY_FILES,
    loading: { music: false, picture: false, recorder: false },
    actionError: '',
  })

  const setKindLoading = (kind: MediaKind, value: boolean) => {
    update((state) => ({
      ...state,
      loading: {
        ...state.loading,
        [kind]: value,
      },
    }))
  }

  const setActionError = (message: string) => {
    update((state) => ({ ...state, actionError: message }))
  }

  const refreshKind = async (kind: MediaKind) => {
    setKindLoading(kind, true)
    try {
      const files = await getMediaFiles(kind)
      update((state) => ({
        ...state,
        files: {
          ...state.files,
          [kind]: files,
        },
      }))
      setActionError('')
    } catch (error) {
      setActionError(`${kind}: ${toErrorMessage(error)}`)
    } finally {
      setKindLoading(kind, false)
    }
  }

  const refreshAll = async () => {
    await Promise.all((Object.keys(EMPTY_FILES) as MediaKind[]).map((kind) => refreshKind(kind)))
  }

  const play = async (path: string) => {
    if (!path.trim()) {
      return
    }

    const runtime = get(runtimeStore)
    if (runtime.media?.playing) {
      throw new Error('Lecture déjà active. Stop requis avant un nouveau play.')
    }

    await runtimeStore.runAction(`play:${path}`, async () => {
      await playMedia(path)
    })
    await runtimeStore.refreshSnapshot()
  }

  const stop = async () => {
    await runtimeStore.runAction('mediaStop', stopMedia)
    await runtimeStore.refreshSnapshot()
  }

  const recordStart = async (seconds: number, filename: string) => {
    await runtimeStore.runAction('record', async () => {
      await startRecord(seconds, filename)
    })
    await runtimeStore.refreshSnapshot()
  }

  const recordStop = async () => {
    await runtimeStore.runAction('record', stopRecord)
    await runtimeStore.refreshSnapshot()
  }

  const reset = () => {
    set({
      files: EMPTY_FILES,
      loading: { music: false, picture: false, recorder: false },
      actionError: '',
    })
  }

  return {
    subscribe,
    refreshKind,
    refreshAll,
    play,
    stop,
    recordStart,
    recordStop,
    setActionError,
    reset,
  }
}

export const mediaStore = createMediaStore()
export type { MediaStoreState, MediaFilesMap }
