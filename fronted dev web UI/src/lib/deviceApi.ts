import type { ScenarioMeta, StreamMessage } from '../types/story'

export type ApiFlavor = 'story_v2' | 'freenove_legacy' | 'unknown'
export type StreamKind = 'ws' | 'sse' | 'none'

export type DeviceCapabilities = {
  canSelectScenario: boolean
  canStart: boolean
  canPause: boolean
  canResume: boolean
  canSkip: boolean
  canValidate: boolean
  canDeploy: boolean
  canNetworkControl: boolean
  streamKind: StreamKind
}

export type ApiError = Error & {
  status?: number
  detail?: string
  code?: string
  flavor?: ApiFlavor
  base?: string
}

export type RuntimeInfo = {
  base: string
  flavor: ApiFlavor
  capabilities: DeviceCapabilities
}

export type StreamStatus = 'connecting' | 'open' | 'closed' | 'error'

export type ConnectStreamOptions = {
  onMessage: (message: StreamMessage) => void
  onStatus?: (status: StreamStatus) => void
}

export type StreamConnection = {
  kind: StreamKind
  close: () => void
}

const DEFAULT_TIMEOUT_MS = 6000
const FLAVOR_OVERRIDE = (import.meta.env.VITE_API_FLAVOR ?? 'auto').toLowerCase()
const PROBE_PORTS_RAW = import.meta.env.VITE_API_PROBE_PORTS ?? '80,8080'
const EXPLICIT_BASE = import.meta.env.VITE_API_BASE ?? ''

export const API_BASE =
  EXPLICIT_BASE ||
  (typeof window !== 'undefined'
    ? `${window.location.protocol}//${window.location.hostname}`
    : 'http://localhost')

const CAPABILITIES: Record<ApiFlavor, DeviceCapabilities> = {
  story_v2: {
    canSelectScenario: true,
    canStart: true,
    canPause: true,
    canResume: true,
    canSkip: true,
    canValidate: true,
    canDeploy: true,
    canNetworkControl: false,
    streamKind: 'ws',
  },
  freenove_legacy: {
    canSelectScenario: false,
    canStart: false,
    canPause: false,
    canResume: false,
    canSkip: true,
    canValidate: false,
    canDeploy: false,
    canNetworkControl: true,
    streamKind: 'sse',
  },
  unknown: {
    canSelectScenario: false,
    canStart: false,
    canPause: false,
    canResume: false,
    canSkip: false,
    canValidate: false,
    canDeploy: false,
    canNetworkControl: false,
    streamKind: 'none',
  },
}

let runtimeCache: RuntimeInfo | null = null

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null

const ensureHttp = (value: string) => (/^https?:\/\//i.test(value) ? value : `http://${value}`)

const stripTrailingSlash = (value: string) => value.replace(/\/+$/, '')

const parseProbePorts = (raw: string) => {
  const ports = raw
    .split(',')
    .map((part) => Number.parseInt(part.trim(), 10))
    .filter((port) => Number.isInteger(port) && port >= 1 && port <= 65535)
  return ports.length > 0 ? Array.from(new Set(ports)) : [80, 8080]
}

const getBaseCandidates = (baseHint?: string) => {
  const primary = stripTrailingSlash(ensureHttp(baseHint ?? API_BASE))
  const primaryUrl = new URL(primary)
  const hostRoot = `${primaryUrl.protocol}//${primaryUrl.hostname}`
  const candidates: string[] = []
  const push = (candidate: string) => {
    const normalized = stripTrailingSlash(candidate)
    if (!candidates.includes(normalized)) {
      candidates.push(normalized)
    }
  }

  push(primary)
  for (const port of parseProbePorts(PROBE_PORTS_RAW)) {
    push(`${hostRoot}:${port}`)
  }
  return candidates
}

const createApiError = (
  message: string,
  extras: Partial<Pick<ApiError, 'status' | 'detail' | 'code' | 'flavor' | 'base'>> = {},
) => {
  const error = new Error(message) as ApiError
  error.status = extras.status
  error.detail = extras.detail
  error.code = extras.code
  error.flavor = extras.flavor
  error.base = extras.base
  return error
}

const normalizeErrorBody = (body: unknown) => {
  if (!isRecord(body)) {
    return { message: '', detail: '', code: '' }
  }

  const fromError = body.error
  if (isRecord(fromError)) {
    const message = typeof fromError.message === 'string' ? fromError.message : ''
    const detail =
      typeof fromError.details === 'string'
        ? fromError.details
        : typeof fromError.detail === 'string'
          ? fromError.detail
          : ''
    const code =
      typeof fromError.code === 'string' || typeof fromError.code === 'number'
        ? String(fromError.code)
        : ''
    return { message, detail, code }
  }

  if (typeof fromError === 'string') {
    return { message: fromError, detail: '', code: '' }
  }

  return {
    message: typeof body.message === 'string' ? body.message : '',
    detail:
      typeof body.detail === 'string'
        ? body.detail
        : typeof body.details === 'string'
          ? body.details
          : '',
    code: typeof body.code === 'string' ? body.code : '',
  }
}

const fetchWithTimeout = async (url: string, options: RequestInit = {}, timeoutMs = DEFAULT_TIMEOUT_MS) => {
  const controller = new AbortController()
  const timeout = window.setTimeout(() => controller.abort(), timeoutMs)
  try {
    return await fetch(url, {
      ...options,
      signal: controller.signal,
    })
  } finally {
    window.clearTimeout(timeout)
  }
}

const parseResponseBody = async (response: Response): Promise<unknown> => {
  const contentType = response.headers.get('content-type') ?? ''
  if (contentType.includes('application/json')) {
    return response.json()
  }

  const text = await response.text()
  if (!text) {
    return null
  }
  try {
    return JSON.parse(text) as unknown
  } catch {
    return text
  }
}

const requestJsonAt = async (
  base: string,
  flavor: ApiFlavor,
  path: string,
  options: RequestInit = {},
): Promise<unknown> => {
  const hasBody = options.body !== undefined && options.body !== null
  const headers: HeadersInit = {
    Accept: 'application/json',
    ...(hasBody ? { 'Content-Type': 'application/json' } : {}),
    ...(options.headers ?? {}),
  }

  let response: Response
  try {
    response = await fetchWithTimeout(`${base}${path}`, { ...options, headers })
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Network request failed'
    throw createApiError(message, { flavor, base })
  }

  const body = await parseResponseBody(response)
  if (!response.ok) {
    const normalized = normalizeErrorBody(body)
    throw createApiError(normalized.message || response.statusText || 'Request failed', {
      status: response.status,
      detail: normalized.detail,
      code: normalized.code,
      flavor,
      base,
    })
  }

  if (isRecord(body) && body.ok === false) {
    const normalized = normalizeErrorBody(body)
    throw createApiError(normalized.message || normalized.detail || 'Device reported an error', {
      detail: normalized.detail,
      code: normalized.code || 'device_error',
      flavor,
      base,
      status: response.status,
    })
  }

  return body
}

const probeJson = async (base: string, path: string) => {
  try {
    const response = await fetchWithTimeout(`${base}${path}`, { method: 'GET' }, 2200)
    const body = await parseResponseBody(response)
    return { ok: response.ok, body }
  } catch {
    return { ok: false, body: null as unknown }
  }
}

const detectRuntime = async (baseHint?: string): Promise<{ base: string; flavor: ApiFlavor }> => {
  const forcedFlavor =
    FLAVOR_OVERRIDE === 'story_v2' || FLAVOR_OVERRIDE === 'freenove_legacy'
      ? (FLAVOR_OVERRIDE as ApiFlavor)
      : null

  const candidates = getBaseCandidates(baseHint)
  if (forcedFlavor) {
    return { base: candidates[0], flavor: forcedFlavor }
  }

  for (const base of candidates) {
    const storyProbe = await probeJson(base, '/api/story/list')
    if (storyProbe.ok && (Array.isArray(storyProbe.body) || (isRecord(storyProbe.body) && Array.isArray(storyProbe.body.scenarios)))) {
      return { base, flavor: 'story_v2' }
    }

    const legacyProbe = await probeJson(base, '/api/status')
    if (legacyProbe.ok && isRecord(legacyProbe.body) && (isRecord(legacyProbe.body.story) || isRecord(legacyProbe.body.network))) {
      return { base, flavor: 'freenove_legacy' }
    }
  }

  return { base: candidates[0], flavor: 'unknown' }
}

const unsupported = (runtime: RuntimeInfo, message: string): never => {
  throw createApiError(message, {
    code: 'unsupported_capability',
    flavor: runtime.flavor,
    base: runtime.base,
  })
}

const parseScenario = (value: unknown): ScenarioMeta | null => {
  if (!isRecord(value)) {
    return null
  }
  const idValue = value.id ?? value.scenario_id ?? value.scenario
  if (typeof idValue !== 'string' || idValue.trim().length === 0) {
    return null
  }

  const estimated = typeof value.estimated_duration_s === 'number' ? value.estimated_duration_s : undefined
  const duration = typeof value.duration_s === 'number' ? value.duration_s : undefined
  const description = typeof value.description === 'string' ? value.description : undefined
  const step = typeof value.current_step === 'string' ? value.current_step : undefined

  return {
    id: idValue,
    estimated_duration_s: estimated,
    duration_s: duration,
    description,
    current_step: step,
  }
}

const getRuntime = async (): Promise<RuntimeInfo> => {
  if (runtimeCache) {
    return runtimeCache
  }
  const detected = await detectRuntime()
  runtimeCache = {
    base: detected.base,
    flavor: detected.flavor,
    capabilities: CAPABILITIES[detected.flavor],
  }
  return runtimeCache
}

export const detectFlavor = async (baseUrl?: string) => {
  const detected = await detectRuntime(baseUrl)
  return detected.flavor
}

export const getCapabilities = (flavor: ApiFlavor) => CAPABILITIES[flavor]

export const getRuntimeInfo = async () => getRuntime()

export const listScenarios = async (): Promise<ScenarioMeta[]> => {
  const runtime = await getRuntime()
  if (runtime.flavor === 'story_v2') {
    const body = await requestJsonAt(runtime.base, runtime.flavor, '/api/story/list')
    const entries = Array.isArray(body)
      ? body
      : isRecord(body) && Array.isArray(body.scenarios)
        ? body.scenarios
        : []
    return entries.map(parseScenario).filter((scenario): scenario is ScenarioMeta => scenario !== null)
  }

  if (runtime.flavor === 'freenove_legacy') {
    const body = await requestJsonAt(runtime.base, runtime.flavor, '/api/status')
    if (!isRecord(body) || !isRecord(body.story)) {
      return []
    }
    const scenarioId =
      typeof body.story.scenario === 'string' && body.story.scenario.length > 0 ? body.story.scenario : 'DEFAULT'
    const currentStep = typeof body.story.step === 'string' ? body.story.step : undefined
    return [
      {
        id: scenarioId,
        is_current: true,
        current_step: currentStep,
        description: currentStep ? `Current device step: ${currentStep}` : 'Current scenario on device',
      },
    ]
  }

  return unsupported(runtime, `No compatible API detected at ${runtime.base}.`)
  return []
}

export const selectStory = async (id: string) => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'story_v2') {
    unsupported(runtime, 'Scenario selection is unavailable in legacy mode.')
  }
  return requestJsonAt(runtime.base, runtime.flavor, `/api/story/select/${encodeURIComponent(id)}`, {
    method: 'POST',
  })
}

export const startStory = async () => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'story_v2') {
    unsupported(runtime, 'Start is unavailable in legacy mode.')
  }
  return requestJsonAt(runtime.base, runtime.flavor, '/api/story/start', { method: 'POST' })
}

export const pauseStory = async () => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'story_v2') {
    unsupported(runtime, 'Pause is unavailable in legacy mode.')
  }
  return requestJsonAt(runtime.base, runtime.flavor, '/api/story/pause', { method: 'POST' })
}

export const resumeStory = async () => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'story_v2') {
    unsupported(runtime, 'Resume is unavailable in legacy mode.')
  }
  return requestJsonAt(runtime.base, runtime.flavor, '/api/story/resume', { method: 'POST' })
}

export const skipStory = async () => {
  const runtime = await getRuntime()
  if (runtime.flavor === 'story_v2') {
    return requestJsonAt(runtime.base, runtime.flavor, '/api/story/skip', { method: 'POST' })
  }
  if (runtime.flavor === 'freenove_legacy') {
    return requestJsonAt(runtime.base, runtime.flavor, '/api/scenario/next', { method: 'POST' })
  }
  unsupported(runtime, 'Skip is unavailable because API flavor is unknown.')
}

export const validateStory = async (yaml: string) => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'story_v2') {
    unsupported(runtime, 'Validation is unavailable in legacy mode.')
  }
  return requestJsonAt(runtime.base, runtime.flavor, '/api/story/validate', {
    method: 'POST',
    body: JSON.stringify({ yaml }),
  })
}

export const deployStory = async (yaml: string) => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'story_v2') {
    unsupported(runtime, 'Deployment is unavailable in legacy mode.')
  }
  return requestJsonAt(runtime.base, runtime.flavor, '/api/story/deploy', {
    method: 'POST',
    body: JSON.stringify({ yaml }),
  })
}

export const wifiReconnect = async () => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'freenove_legacy') {
    unsupported(runtime, 'WiFi reconnect control is unavailable outside legacy mode.')
  }
  return requestJsonAt(runtime.base, runtime.flavor, '/api/network/wifi/reconnect', { method: 'POST' })
}

export const setEspNowEnabled = async (enabled: boolean) => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'freenove_legacy') {
    unsupported(runtime, 'ESP-NOW control is unavailable outside legacy mode.')
  }
  const path = enabled ? '/api/network/espnow/on' : '/api/network/espnow/off'
  return requestJsonAt(runtime.base, runtime.flavor, path, { method: 'POST' })
}

const parseStreamMessage = (raw: unknown): StreamMessage => {
  if (!isRecord(raw) || typeof raw.type !== 'string') {
    return { type: 'error', data: { message: 'Invalid stream payload' } }
  }
  return {
    type: raw.type,
    data: isRecord(raw.data) ? raw.data : undefined,
    ts: typeof raw.ts === 'string' ? raw.ts : typeof raw.timestamp === 'string' ? raw.timestamp : undefined,
  }
}

export const connectStream = async (options: ConnectStreamOptions): Promise<StreamConnection> => {
  const runtime = await getRuntime()
  const { onMessage, onStatus } = options

  if (runtime.flavor === 'story_v2') {
    const wsUrl = `${runtime.base.replace(/^http/i, 'ws')}/api/story/stream`
    onStatus?.('connecting')
    const socket = new WebSocket(wsUrl)
    socket.onopen = () => onStatus?.('open')
    socket.onmessage = (event) => {
      try {
        const payload = JSON.parse(event.data) as unknown
        onMessage(parseStreamMessage(payload))
      } catch {
        onMessage({ type: 'error', data: { message: 'Invalid stream payload' } })
      }
    }
    socket.onerror = () => onStatus?.('error')
    socket.onclose = () => onStatus?.('closed')

    return {
      kind: 'ws',
      close: () => {
        socket.onopen = null
        socket.onmessage = null
        socket.onerror = null
        socket.onclose = null
        socket.close()
      },
    }
  }

  if (runtime.flavor === 'freenove_legacy') {
    const sseUrl = `${runtime.base}/api/stream`
    onStatus?.('connecting')
    const source = new EventSource(sseUrl)
    source.onopen = () => onStatus?.('open')
    source.onerror = () => onStatus?.('error')
    source.addEventListener('status', (event) => {
      const messageEvent = event as MessageEvent
      try {
        const payload = JSON.parse(messageEvent.data) as Record<string, unknown>
        onMessage({ type: 'status', data: payload })
      } catch {
        onMessage({ type: 'error', data: { message: 'Invalid legacy stream payload' } })
      }
    })
    source.addEventListener('done', () => onStatus?.('closed'))

    return {
      kind: 'sse',
      close: () => {
        source.close()
      },
    }
  }

  throw createApiError('Streaming is unavailable because API flavor is unknown.', {
    code: 'unsupported_capability',
    flavor: runtime.flavor,
    base: runtime.base,
  })
}
