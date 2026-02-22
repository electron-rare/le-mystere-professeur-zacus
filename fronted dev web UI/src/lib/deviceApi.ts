import type { ScenarioMeta, StreamMessage } from '../types/story'

export type ApiFlavor = 'story_v2' | 'freenove_legacy' | 'unknown'
export type StreamKind = 'ws' | 'sse' | 'none'

type ProbeMethod = 'GET' | 'OPTIONS'

type EndpointProbe = {
  path: string
  method: ProbeMethod
  status: number
  routeExists: boolean
  allowedMethods: string[]
  note: string
}

export type DeviceCapabilities = {
  canSelectScenario: boolean
  canStart: boolean
  canPause: boolean
  canResume: boolean
  canSkip: boolean
  canValidate: boolean
  canDeploy: boolean
  canNetworkControl: boolean
  canFirmwareInfo: boolean
  canFirmwareUpdate: boolean
  canFirmwareReboot: boolean
  streamKind: StreamKind
}

export type FirmwareInfo = {
  flavor: ApiFlavor
  version?: string
  versionPath?: string
  canFirmwareInfo: boolean
  canFirmwareUpdate: boolean
  canFirmwareReboot: boolean
  updateEndpoints: string[]
  rebootEndpoints: string[]
  versionChecks: EndpointProbe[]
  updateChecks: EndpointProbe[]
  rebootChecks: EndpointProbe[]
  warnings: string[]
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
  firmwareInfo: FirmwareInfo
  legacyControlSupport?: LegacyControlSupport
}

type LegacyControlSupport = {
  scenarioNext: boolean
  scenarioUnlock: boolean
  control: boolean
  wifiReconnect: boolean
  espNowOn: boolean
  espNowOff: boolean
}

export type StoryRuntimeState = 'running' | 'paused' | 'done' | 'idle' | 'stopped' | 'unknown'

export type StoryRuntimeStatus = {
  scenarioId?: string
  currentStep?: string
  status: StoryRuntimeState
  progressPct: number
  source: ApiFlavor
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
const FIRMWARE_PROBE_TIMEOUT_MS = 2200
const FIRMWARE_VERSION_PATHS = [
  '/api/version',
  '/api/firmware',
  '/api/system/version',
  '/api/system/info',
  '/api/info',
  '/api/status',
] as const
const FIRMWARE_UPDATE_PATHS = [
  '/api/update',
  '/api/ota',
  '/api/ota/update',
  '/api/system/update',
  '/api/upgrade',
  '/api/upgrade/firmware',
] as const
const FIRMWARE_REBOOT_PATHS = [
  '/api/reboot',
  '/api/system/reboot',
  '/api/reset',
  '/api/restart',
] as const
const LEGACY_CONTROL_PATHS = [
  '/api/scenario/next',
  '/api/scenario/unlock',
  '/api/control',
  '/api/network/wifi/reconnect',
  '/api/network/espnow/on',
  '/api/network/espnow/off',
] as const
const LEGACY_NETWORK_STATUS_PATHS = ['/api/network/wifi', '/api/network/espnow'] as const

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
    canFirmwareInfo: true,
    canFirmwareUpdate: false,
    canFirmwareReboot: false,
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
    canFirmwareInfo: true,
    canFirmwareUpdate: false,
    canFirmwareReboot: false,
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
    canFirmwareInfo: false,
    canFirmwareUpdate: false,
    canFirmwareReboot: false,
    streamKind: 'none',
  },
}

let runtimeCache: RuntimeInfo | null = null

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null

const stripVersion = (value: unknown) => {
  if (typeof value !== 'string') {
    return ''
  }
  const normalized = value.trim()
  if (!normalized) {
    return ''
  }
  const match = normalized.match(/\b\d+\.\d+(?:\.\d+)?(?:[-._][A-Za-z0-9._-]+)?\b/)
  return match ? match[0] : normalized
}

const parseFirmwareVersionFromRecord = (value: Record<string, unknown>): string => {
  const directFields = [
    'version',
    'version_name',
    'firmware',
    'firmware_version',
    'fw_version',
    'build_version',
  ] as const

  for (const field of directFields) {
    const parsed = stripVersion(value[field])
    if (parsed) {
      return parsed
    }
  }

  const nestedFields = ['system', 'meta', 'info', 'build', 'hardware'] as const
  for (const field of nestedFields) {
    const nested = value[field]
    if (isRecord(nested)) {
      const parsed: string = parseFirmwareVersionFromRecord(nested)
      if (parsed) {
        return parsed
      }
    }
  }

  return ''
}

const asString = (value: unknown) => (typeof value === 'string' ? value.trim() : '')

const parseStoryState = (value: unknown): StoryRuntimeState => {
  const normalized = asString(value).toLowerCase()
  if (normalized === 'running' || normalized === 'paused' || normalized === 'done' || normalized === 'idle' || normalized === 'stopped') {
    return normalized
  }
  return 'unknown'
}

const firstKnownStatus = (states: Array<unknown>) => {
  for (const entry of states) {
    const parsed = parseStoryState(entry)
    if (parsed !== 'unknown') {
      return parsed
    }
  }
  return 'unknown'
}

const normalizeProgress = (value: unknown) => {
  const parsed = typeof value === 'number' ? value : typeof value === 'string' ? Number(value) : Number.NaN
  if (Number.isFinite(parsed)) {
    return Math.max(0, Math.min(100, Math.round(parsed)))
  }
  return 0
}

const pickFirstString = (value: Record<string, unknown>, keys: string[]) => {
  for (const key of keys) {
    const next = asString(value[key])
    if (next) {
      return next
    }
  }
  return ''
}

const normalizeAllowedMethods = (value: string | null) => {
  if (!value) {
    return [] as string[]
  }
  return value
    .split(',')
    .map((entry) => entry.trim().toUpperCase())
    .filter(Boolean)
}

const isEndpointReachable = (probe: EndpointProbe) => probe.status !== 0 && probe.status !== 404

const probeSupportsPost = (probe: EndpointProbe) => {
  if (!isEndpointReachable(probe)) {
    return false
  }
  if (probe.method === 'OPTIONS' && probe.allowedMethods.length > 0) {
    return probe.allowedMethods.includes('POST')
  }
  return isEndpointReachable(probe)
}

const probeCandidate = async (base: string, path: string, method: ProbeMethod): Promise<EndpointProbe> => {
  try {
    const response = await fetchWithTimeout(`${base}${path}`, { method }, FIRMWARE_PROBE_TIMEOUT_MS)
    const routeExists = response.status !== 404
    const allowedMethods = method === 'OPTIONS' ? normalizeAllowedMethods(response.headers.get('Allow')) : []
    return {
      path,
      method,
      status: response.status,
      routeExists,
      allowedMethods,
      note: `${response.status} ${response.statusText || ''}`.trim(),
    }
  } catch {
    return {
      path,
      method,
      status: 0,
      routeExists: false,
      allowedMethods: [],
      note: 'unreachable',
    }
  }
}

const probePostSupport = async (base: string, path: string) => {
  const optionsProbe = await probeCandidate(base, path, 'OPTIONS')
  if (probeSupportsPost(optionsProbe)) {
    return true
  }

  const getProbe = await probeCandidate(base, path, 'GET')
  if (getProbe.status !== 404 && getProbe.status !== 0) {
    return true
  }

  return false
}

const probeCandidates = async (base: string, paths: readonly string[], methods: readonly ProbeMethod[]) => {
  const tasks: Promise<EndpointProbe>[] = []
  for (const path of paths) {
    for (const method of methods) {
      tasks.push(probeCandidate(base, path, method))
    }
  }
  return Promise.all(tasks)
}

const detectFirmwareInfo = async (base: string): Promise<FirmwareInfo> => {
  const versionProbes = await probeCandidates(base, FIRMWARE_VERSION_PATHS, ['GET'])
  const versionChecks: EndpointProbe[] = [...versionProbes]
  const bodyCache = new Map<string, unknown>()

  const loadBody = async (path: string) => {
    if (bodyCache.has(path)) {
      return bodyCache.get(path)
    }
    try {
      const response = await fetchWithTimeout(`${base}${path}`, {}, FIRMWARE_PROBE_TIMEOUT_MS)
      const body = await parseResponseBody(response)
      bodyCache.set(path, body)
      return body
    } catch {
      return null
    }
  }

  let version = ''
  let versionPath = ''
  for (const probe of versionProbes) {
    if (!probe.routeExists || version || probe.status < 200 || probe.status >= 500) {
      continue
    }
    const candidate = await loadBody(probe.path)
    if (isRecord(candidate)) {
      const parsed = parseFirmwareVersionFromRecord(candidate)
      if (parsed) {
        version = parsed
        versionPath = probe.path
      }
      continue
    }

    const parsed = stripVersion(candidate)
    if (parsed) {
      version = parsed
      versionPath = probe.path
    }
  }

  const updateChecks = await probeCandidates(base, FIRMWARE_UPDATE_PATHS, ['GET', 'OPTIONS'])
  const rebootChecks = await probeCandidates(base, FIRMWARE_REBOOT_PATHS, ['GET', 'OPTIONS'])

  const isPostCapable = (probe: EndpointProbe) => {
    if (!probe.routeExists) {
      return false
    }
    if (probe.method === 'OPTIONS') {
      return probe.allowedMethods.includes('POST')
    }
    if (probe.status >= 400) {
      return false
    }
    return true
  }

  const updateEndpoints = [...new Set(updateChecks.filter(isPostCapable).map((probe) => probe.path))]
  const rebootEndpoints = [...new Set(rebootChecks.filter(isPostCapable).map((probe) => probe.path))]
  const firmwareRouteFound = versionChecks.some((probe) => probe.routeExists) || updateChecks.some((probe) => probe.routeExists) || rebootChecks.some((probe) => probe.routeExists)

  const warnings: string[] = []
  if (!version) {
    warnings.push('Version non exposée par cette API.')
  }
  if (updateEndpoints.length === 0) {
    warnings.push('Aucun endpoint de mise à jour OTA détecté.')
  }
  if (rebootEndpoints.length === 0) {
    warnings.push('Aucun endpoint de redémarrage détecté.')
  }

  return {
    flavor: firmwareRouteFound ? 'freenove_legacy' : 'unknown',
    version,
    versionPath: versionPath || undefined,
    canFirmwareInfo: firmwareRouteFound,
    canFirmwareUpdate: updateEndpoints.length > 0,
    canFirmwareReboot: rebootEndpoints.length > 0,
    updateEndpoints,
    rebootEndpoints,
    versionChecks,
    updateChecks,
    rebootChecks,
    warnings,
  }
}

const detectLegacyControlSupport = async (base: string): Promise<LegacyControlSupport> => {
  const checks = await Promise.all(LEGACY_CONTROL_PATHS.map((path) => probePostSupport(base, path)))
  const networkChecks = await Promise.all(LEGACY_NETWORK_STATUS_PATHS.map((path) => probeCandidate(base, path, 'GET')))
  const values = Object.fromEntries(
    LEGACY_CONTROL_PATHS.map((path, index) => [path, checks[index] ?? false]),
  ) as Record<(typeof LEGACY_CONTROL_PATHS)[number], boolean>
  const networkStatusMap = Object.fromEntries(
    LEGACY_NETWORK_STATUS_PATHS.map((path, index) => [path, networkChecks[index]?.routeExists ?? false]),
  ) as Record<(typeof LEGACY_NETWORK_STATUS_PATHS)[number], boolean>
  const hasNetworkStatus = networkStatusMap['/api/network/wifi']
  const hasEspNowStatus = networkStatusMap['/api/network/espnow']

  return {
    scenarioNext: values['/api/scenario/next'],
    scenarioUnlock: values['/api/scenario/unlock'],
    control: values['/api/control'],
    wifiReconnect: values['/api/network/wifi/reconnect'] || hasNetworkStatus,
    espNowOn: values['/api/network/espnow/on'] || hasEspNowStatus,
    espNowOff: values['/api/network/espnow/off'] || hasEspNowStatus,
  }
}

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

const normalizeLegacyStatusPayload = (body: Record<string, unknown>): Omit<StoryRuntimeStatus, 'source'> => {
  const story = isRecord(body.story) ? body.story : {}
  return {
    scenarioId: pickFirstString(story, ['scenario', 'id']),
    currentStep: pickFirstString(story, ['step', 'current_step', 'current']),
    status: firstKnownStatus([story.status, story.state, body.status, body.state]),
    progressPct: 0,
  }
}

const normalizeStoryV2StatusPayload = (body: Record<string, unknown>): Omit<StoryRuntimeStatus, 'source'> => {
  return {
    scenarioId: pickFirstString(body, ['selected', 'scenario_id', 'scenario']),
    currentStep: pickFirstString(body, ['current_step', 'currentStep']),
    status: firstKnownStatus([body.status, body.state]),
    progressPct: normalizeProgress(body.progress_pct),
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
  const firmwareInfo = await detectFirmwareInfo(detected.base)
  const legacyControlSupport =
    detected.flavor === 'freenove_legacy' ? await detectLegacyControlSupport(detected.base) : undefined
  const baseCapabilities = CAPABILITIES[detected.flavor]
  const capabilities: DeviceCapabilities = {
    ...baseCapabilities,
    canSkip:
      detected.flavor === 'freenove_legacy'
        ? Boolean(legacyControlSupport && (legacyControlSupport.scenarioNext || legacyControlSupport.control))
        : baseCapabilities.canSkip,
    canNetworkControl:
      detected.flavor === 'freenove_legacy'
        ? Boolean(
            legacyControlSupport &&
              (legacyControlSupport.wifiReconnect || legacyControlSupport.espNowOn || legacyControlSupport.espNowOff),
          )
        : baseCapabilities.canNetworkControl,
    canFirmwareInfo: firmwareInfo.canFirmwareInfo,
    canFirmwareUpdate: firmwareInfo.canFirmwareUpdate,
    canFirmwareReboot: firmwareInfo.canFirmwareReboot,
  }
  runtimeCache = {
    base: detected.base,
    flavor: detected.flavor,
    capabilities,
    firmwareInfo,
    legacyControlSupport,
  }
  return runtimeCache
}

export const detectFlavor = async (baseUrl?: string) => {
  const detected = await detectRuntime(baseUrl)
  return detected.flavor
}

export const getCapabilities = (flavor: ApiFlavor) => CAPABILITIES[flavor]

export const getRuntimeInfo = async () => getRuntime()
export const getFirmwareInfo = async () => {
  const runtime = await getRuntime()
  return runtime.firmwareInfo
}

export const getStoryRuntimeStatus = async (): Promise<StoryRuntimeStatus> => {
  const runtime = await getRuntime()

  if (runtime.flavor === 'story_v2') {
    const body = await requestJsonAt(runtime.base, runtime.flavor, '/api/story/status')
    if (isRecord(body)) {
      const status = normalizeStoryV2StatusPayload(body)
      return {
        ...status,
        source: runtime.flavor,
      }
    }
    throw createApiError('Réponse inattendue de /api/story/status', {
      status: 502,
      flavor: runtime.flavor,
      base: runtime.base,
      code: 'bad_payload',
    })
  }

  if (runtime.flavor === 'freenove_legacy') {
    const body = await requestJsonAt(runtime.base, runtime.flavor, '/api/status')
    if (isRecord(body)) {
      const status = normalizeLegacyStatusPayload(body)
      return {
        ...status,
        source: runtime.flavor,
      }
    }
    throw createApiError('Réponse inattendue de /api/status', {
      status: 502,
      flavor: runtime.flavor,
      base: runtime.base,
      code: 'bad_payload',
    })
  }

  throw createApiError('Le statut runtime n`est pas disponible en mode API inconnu.', {
    code: 'unsupported_capability',
    flavor: runtime.flavor,
    base: runtime.base,
  })
}

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
    if (runtime.legacyControlSupport?.scenarioNext) {
      return requestJsonAt(runtime.base, runtime.flavor, '/api/scenario/next', { method: 'POST' })
    }

    if (runtime.legacyControlSupport?.control) {
      return requestJsonAt(runtime.base, runtime.flavor, '/api/control', {
        method: 'POST',
        body: JSON.stringify({ action: 'NEXT' }),
      })
    }

    unsupported(runtime, 'Skip indisponible: aucune route legacy valide detectee.')
  }
  unsupported(runtime, 'Skip is unavailable because API flavor is unknown.')
}

export const unlockStory = async () => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'freenove_legacy') {
    unsupported(runtime, 'Unlock is unavailable outside legacy mode.')
  }
  if (runtime.legacyControlSupport?.scenarioUnlock) {
    return requestJsonAt(runtime.base, runtime.flavor, '/api/scenario/unlock', { method: 'POST' })
  }
  if (runtime.legacyControlSupport?.control) {
    return requestJsonAt(runtime.base, runtime.flavor, '/api/control', {
      method: 'POST',
      body: JSON.stringify({ action: 'UNLOCK' }),
    })
  }
  unsupported(runtime, 'Unlock is unavailable: route legacy inconnue.')
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
  if (runtime.legacyControlSupport?.wifiReconnect) {
    return requestJsonAt(runtime.base, runtime.flavor, '/api/network/wifi/reconnect', { method: 'POST' })
  }
  if (runtime.legacyControlSupport?.control) {
    return requestJsonAt(runtime.base, runtime.flavor, '/api/control', {
      method: 'POST',
      body: JSON.stringify({ action: 'WIFI_RECONNECT' }),
    })
  }
  unsupported(runtime, 'WiFi reconnect control is unavailable in this legacy firmware.')
}

export const setEspNowEnabled = async (enabled: boolean) => {
  const runtime = await getRuntime()
  if (runtime.flavor !== 'freenove_legacy') {
    unsupported(runtime, 'ESP-NOW control is unavailable outside legacy mode.')
  }
  const path = enabled ? '/api/network/espnow/on' : '/api/network/espnow/off'
  if (enabled ? runtime.legacyControlSupport?.espNowOn : runtime.legacyControlSupport?.espNowOff) {
    return requestJsonAt(runtime.base, runtime.flavor, path, { method: 'POST' })
  }
  if (runtime.legacyControlSupport?.control) {
    return requestJsonAt(runtime.base, runtime.flavor, '/api/control', {
      method: 'POST',
      body: JSON.stringify({ action: enabled ? 'ESPNOW_ON' : 'ESPNOW_OFF' }),
    })
  }
  unsupported(runtime, 'ESP-NOW control is unavailable in this legacy firmware.')
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
