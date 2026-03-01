import { writable } from 'svelte/store'
import {
  API_BASE,
  connectStream,
  getCapabilities,
  getFirmwareInfo,
  getRuntimeInfo,
  listScenarios,
  type ApiFlavor,
  type DeviceCapabilities,
  type FirmwareInfo,
  type StreamConnection,
  type StreamStatus,
} from '../deviceApi'
import {
  getAuditTrail,
  getMediaRuntimeStatus,
  getNetworkSnapshot,
  getRuntimeSnapshot,
  getScenarioList,
  type RuntimeMediaState,
  type RuntimeNetworkState,
  type RuntimeSnapshot,
} from '../runtimeService'
import type { ScenarioMeta, StreamMessage } from '../../types/story'

type UIStreamStatus = StreamStatus | 'inactive' | 'unavailable' | 'recording'

type RuntimeStoreState = {
  loading: boolean
  base: string
  apiFlavor: ApiFlavor
  capabilities: DeviceCapabilities
  firmwareInfo: FirmwareInfo | null
  scenarios: ScenarioMeta[]
  activeScenario: string
  snapshot: RuntimeSnapshot | null
  media: RuntimeMediaState | null
  network: RuntimeNetworkState | null
  streamStatus: UIStreamStatus
  streamEvents: Array<{ at: string; raw: StreamMessage }>
  auditEvents: Array<Record<string, unknown>>
  busy: Record<string, boolean>
  globalError: string
  actionLog: string
}

const defaultState = (): RuntimeStoreState => ({
  loading: true,
  base: API_BASE,
  apiFlavor: 'unknown',
  capabilities: getCapabilities('unknown'),
  firmwareInfo: null,
  scenarios: [],
  activeScenario: '',
  snapshot: null,
  media: null,
  network: null,
  streamStatus: 'inactive',
  streamEvents: [],
  auditEvents: [],
  busy: {},
  globalError: '',
  actionLog: '',
})

const toErrorMessage = (error: unknown) => (error instanceof Error ? error.message : 'Erreur inconnue')

const pickScenario = (scenarios: ScenarioMeta[], current?: string) => {
  if (!scenarios.length) {
    return ''
  }
  if (current && scenarios.some((scenario) => scenario.id === current)) {
    return current
  }
  return scenarios[0]?.id ?? ''
}

const mergeBusy = (current: Record<string, boolean>, key: string, value: boolean) => {
  const next = { ...current }
  if (value) {
    next[key] = true
    return next
  }
  delete next[key]
  return next
}

const createRuntimeStore = () => {
  const { subscribe, update, set } = writable<RuntimeStoreState>(defaultState())

  let pollTimer: ReturnType<typeof setInterval> | null = null
  let streamConn: StreamConnection | null = null

  const setBusy = (key: string, value: boolean) => {
    update((state) => ({
      ...state,
      busy: mergeBusy(state.busy, key, value),
    }))
  }

  const setError = (message: string) => {
    update((state) => ({
      ...state,
      globalError: message,
      actionLog: message,
    }))
  }

  const refreshSnapshot = async () => {
    try {
      const runtime = await getRuntimeInfo()
      const scenarios = await listScenarios()
      const snapshot = await getRuntimeSnapshot()
      const media = await getMediaRuntimeStatus()
      const network = await getNetworkSnapshot()

      update((state) => ({
        ...state,
        loading: false,
        base: runtime.base,
        apiFlavor: runtime.flavor,
        capabilities: runtime.capabilities,
        scenarios,
        activeScenario: snapshot.scenarioId && scenarios.some((entry) => entry.id === snapshot.scenarioId)
          ? snapshot.scenarioId
          : pickScenario(scenarios, state.activeScenario),
        snapshot,
        media,
        network,
        streamStatus: media.recording ? 'recording' : state.streamStatus,
        globalError: '',
      }))
    } catch (error) {
      update((state) => ({ ...state, loading: false, globalError: toErrorMessage(error) }))
    }
  }

  const bootstrap = async () => {
    update((state) => ({ ...state, loading: true, globalError: '' }))

    try {
      const runtime = await getRuntimeInfo()
      const scenarios = await getScenarioList()
      const snapshot = await getRuntimeSnapshot()
      const media = await getMediaRuntimeStatus()
      const network = await getNetworkSnapshot()
      const firmware = await getFirmwareInfo()
      const audit = await getAuditTrail(30)

      update((state) => ({
        ...state,
        loading: false,
        base: runtime.base,
        apiFlavor: runtime.flavor,
        capabilities: runtime.capabilities,
        firmwareInfo: firmware,
        scenarios,
        activeScenario: pickScenario(scenarios, snapshot.scenarioId),
        snapshot,
        media,
        network,
        streamStatus: media.recording ? 'recording' : state.streamStatus,
        auditEvents: audit,
      }))
    } catch (error) {
      setError(toErrorMessage(error))
      update((state) => ({ ...state, loading: false }))
    }
  }

  const startPolling = () => {
    if (pollTimer) {
      return
    }
    pollTimer = setInterval(() => {
      void refreshSnapshot()
    }, 3000)
  }

  const stopPolling = () => {
    if (!pollTimer) {
      return
    }
    clearInterval(pollTimer)
    pollTimer = null
  }

  const connectRuntimeStream = async () => {
    try {
      update((state) => ({ ...state, streamStatus: 'connecting' }))
      streamConn = await connectStream({
        onMessage: (message) => {
          update((state) => ({
            ...state,
            streamEvents: [
              {
                at: new Date().toLocaleTimeString('fr-FR', { hour12: false }),
                raw: message,
              },
              ...state.streamEvents,
            ].slice(0, 80),
          }))
        },
        onStatus: (status) => {
          update((state) => ({ ...state, streamStatus: status }))
        },
      })
    } catch {
      update((state) => ({ ...state, streamStatus: 'unavailable' }))
    }
  }

  const disconnectRuntimeStream = () => {
    if (!streamConn) {
      return
    }
    streamConn.close()
    streamConn = null
    update((state) => ({ ...state, streamStatus: 'inactive' }))
  }

  const runAction = async <T>(key: string, action: () => Promise<T>) => {
    setBusy(key, true)
    update((state) => ({ ...state, globalError: '', actionLog: '' }))
    try {
      const data = await action()
      return data
    } catch (error) {
      const message = toErrorMessage(error)
      setError(message)
      throw error
    } finally {
      setBusy(key, false)
    }
  }

  const refreshAudit = async () => {
    const events = await getAuditTrail(50)
    update((state) => ({ ...state, auditEvents: events }))
  }

  const setActionLog = (message: string) => {
    update((state) => ({ ...state, actionLog: message }))
  }

  const setActiveScenario = (scenarioId: string) => {
    update((state) => ({ ...state, activeScenario: scenarioId }))
  }

  const reset = () => {
    stopPolling()
    disconnectRuntimeStream()
    set(defaultState())
  }

  return {
    subscribe,
    bootstrap,
    refreshSnapshot,
    startPolling,
    stopPolling,
    connectRuntimeStream,
    disconnectRuntimeStream,
    runAction,
    refreshAudit,
    setActionLog,
    setActiveScenario,
    reset,
  }
}

export const runtimeStore = createRuntimeStore()
export type { RuntimeStoreState, UIStreamStatus }
