import { getRuntimeInfo, getStoryRuntimeStatus, listScenarios, type ApiError, type ApiFlavor, type StoryRuntimeStatus } from './deviceApi'
import { requestApi } from './services/api-client'

type RawRecord = Record<string, unknown>

export type MediaKind = 'music' | 'picture' | 'recorder'

export type RuntimeMediaState = {
  ready: boolean
  playing: boolean
  recording: boolean
  record_simulated: boolean
  record_limit_seconds: number
  record_elapsed_seconds: number
  record_file: string
  last_error: string
  media_dirs: {
    music_dir: string
    picture_dir: string
    record_dir: string
  }
}

export type RuntimeNetworkState = {
  wifi: RawRecord
  espnow: RawRecord
  last_error: string
}

export type RuntimeSnapshot = {
  source: ApiFlavor
  scenarioId?: string
  currentStep?: string
  currentScreen?: string
  status: StoryRuntimeStatus['status']
  progressPct: number
  media: RuntimeMediaState
  network?: RuntimeNetworkState
  story?: RawRecord
  rawStatus?: RawRecord
}

export type ApiCallResult<T> = {
  ok: true
  action: string
  data: T
}

export type MediaFileList = {
  kind: MediaKind
  files: string[]
}

const isRecord = (value: unknown): value is RawRecord => typeof value === 'object' && value !== null

const toStringTrimmed = (value: unknown) => (typeof value === 'string' ? value.trim() : '')

const toNumber = (value: unknown, fallback = 0) => {
  if (typeof value === 'number' && Number.isFinite(value)) {
    return Math.max(0, value)
  }
  if (typeof value === 'string') {
    const parsed = Number.parseFloat(value)
    if (Number.isFinite(parsed)) {
      return Math.max(0, parsed)
    }
  }
  return fallback
}

const toBoolean = (value: unknown, fallback = false) => {
  if (typeof value === 'boolean') {
    return value
  }
  if (typeof value === 'string') {
    const normalized = value.trim().toLowerCase()
    if (normalized === 'true' || normalized === '1' || normalized === 'oui') {
      return true
    }
    if (normalized === 'false' || normalized === '0' || normalized === 'non') {
      return false
    }
  }
  return fallback
}

const makeError = (message: string, status?: number) => {
  const error = new Error(message) as ApiError
  error.status = status
  return error
}

const requestJson = async (base: string, path: string, options: RequestInit = {}): Promise<unknown> => {
  const method = options.method || 'GET'
  let body: unknown = undefined
  if (typeof options.body === 'string' && options.body.length > 0) {
    try {
      body = JSON.parse(options.body) as unknown
    } catch {
      body = options.body
    }
  } else if (options.body) {
    body = options.body as unknown
  }

  const result = await requestApi<unknown>({
    action: `${method} ${path}`,
    base,
    path,
    method,
    body,
    headers: (options.headers as Record<string, string> | undefined) ?? {},
  })

  if (!result.ok) {
    throw makeError(result.error, result.status)
  }

  return result.data
}

const normalizeMediaPaths = (raw: unknown): RuntimeMediaState['media_dirs'] => {
  if (!isRecord(raw)) {
    return {
      music_dir: '/music',
      picture_dir: '/picture',
      record_dir: '/recorder',
    }
  }

  return {
    music_dir: toStringTrimmed((raw as RawRecord).music_dir) || '/music',
    picture_dir: toStringTrimmed((raw as RawRecord).picture_dir) || '/picture',
    record_dir: toStringTrimmed((raw as RawRecord).record_dir) || '/recorder',
  }
}

const normalizeMediaState = (raw: unknown): RuntimeMediaState => {
  const source = isRecord(raw)
    ? raw
    : isRecord((raw as RawRecord | undefined)?.media)
      ? ((raw as RawRecord).media as RawRecord)
      : {}

  return {
    ready: toBoolean(source.ready, false),
    playing: toBoolean(source.playing, false),
    recording: toBoolean(source.recording, false),
    record_simulated: toBoolean(source.record_simulated, true),
    record_limit_seconds: toNumber(source.record_limit_seconds, 0),
    record_elapsed_seconds: toNumber(source.record_elapsed_seconds, 0),
    record_file: toStringTrimmed(source.record_file),
    last_error: toStringTrimmed(source.last_error),
    media_dirs: normalizeMediaPaths((source as RawRecord).media_dirs),
  }
}

const normalizeNetworkState = (raw: RawRecord | undefined): RuntimeNetworkState => {
  const wifi = isRecord(raw?.wifi) ? raw?.wifi : {}
  const espnow = isRecord(raw?.espnow) ? raw?.espnow : {}
  const mediaLike = isRecord(raw?.media) ? raw.media : {}

  return {
    wifi,
    espnow,
    last_error: toStringTrimmed((raw as RawRecord | undefined)?.network_error || mediaLike.last_error),
  }
}

const normalizeProgress = (raw: RawRecord | undefined) => {
  const fromStatus = toNumber(raw?.progress_pct, 0)
  if (fromStatus > 0) {
    return Math.max(0, Math.min(100, Math.round(fromStatus)))
  }
  return 0
}

const normalizeStatusPayload = async (base: string): Promise<RawRecord | null> => {
  try {
    const payload = await requestJson(base, '/api/status')
    return isRecord(payload) ? payload : null
  } catch {
    return null
  }
}

export const getRuntimeSnapshot = async (): Promise<RuntimeSnapshot> => {
  const runtime = await getRuntimeInfo()
  const snapshot = await getStoryRuntimeStatus()
  const statusPayload = await normalizeStatusPayload(runtime.base)
  const storyPayload = isRecord(statusPayload) && isRecord(statusPayload.story) ? statusPayload.story : {}

  const mediaFromRecord = await getMediaRuntimeStatus()
  const mediaFromStatus = normalizeMediaState(storyPayload.media)

  return {
    source: runtime.flavor,
    scenarioId: snapshot.scenarioId || toStringTrimmed(storyPayload.scenario || storyPayload.id || storyPayload.selected),
    currentStep:
      snapshot.currentStep ||
      toStringTrimmed(storyPayload.current_step || storyPayload.current || storyPayload.step || storyPayload.scene) ||
      'inconnu',
    currentScreen: toStringTrimmed(storyPayload.screen || storyPayload.scene),
    status: snapshot.status,
    progressPct: normalizeProgress(storyPayload as RawRecord) || snapshot.progressPct,
    media: {
      ...mediaFromStatus,
      ...mediaFromRecord,
    },
    network: normalizeNetworkState(statusPayload || undefined),
    story: storyPayload,
    rawStatus: statusPayload || undefined,
  }
}

export const getMediaFiles = async (kind: MediaKind): Promise<string[]> => {
  const runtime = await getRuntimeInfo()
  const payload = await requestJson(runtime.base, `/api/media/files?kind=${encodeURIComponent(kind)}`)
  if (!isRecord(payload)) {
    throw makeError('Réponse media invalide.')
  }

  const rawFiles = payload.files
  if (!Array.isArray(rawFiles)) {
    return []
  }

  return rawFiles
    .map((entry) => (typeof entry === 'string' ? entry.trim() : ''))
    .filter((entry): entry is string => Boolean(entry))
}

export const playMedia = async (pathOrFile: string) => {
  const runtime = await getRuntimeInfo()
  const body = toStringTrimmed(pathOrFile)
  const payload = body.includes('/') ? { path: body } : { file: body }
  const actionName = 'MEDIA_PLAY'

  try {
    const data = await requestJson(runtime.base, '/api/media/play', {
      method: 'POST',
      body: JSON.stringify(payload),
    })
    return { ok: true, action: actionName, data: isRecord(data) ? data : {} }
  } catch {
    try {
      const fallback = await requestJson(runtime.base, '/api/control', {
        method: 'POST',
        body: JSON.stringify({ action: `${actionName} ${body}` }),
      })
      return { ok: true, action: actionName, data: isRecord(fallback) ? fallback : {} }
    } catch {
      throw makeError('Lecture media impossible.')
    }
  }
}

export const stopMedia = async () => {
  const runtime = await getRuntimeInfo()
  const actionName = 'MEDIA_STOP'

  try {
    const data = await requestJson(runtime.base, '/api/media/stop', { method: 'POST' })
    return { ok: true, action: actionName, data: isRecord(data) ? data : {} }
  } catch {
    try {
      const fallback = await requestJson(runtime.base, '/api/control', {
        method: 'POST',
        body: JSON.stringify({ action: actionName }),
      })
      return { ok: true, action: actionName, data: isRecord(fallback) ? fallback : {} }
    } catch {
      throw makeError('Arrêt media impossible.')
    }
  }
}

export const startRecord = async (seconds: number, filename: string) => {
  const runtime = await getRuntimeInfo()
  const actionName = 'REC_START'
  const body = {
    seconds: Math.max(1, Math.floor(seconds || 0)),
    filename: toStringTrimmed(filename) || undefined,
  }

  try {
    const data = await requestJson(runtime.base, '/api/media/record/start', {
      method: 'POST',
      body: JSON.stringify(body),
    })
    return { ok: true, action: actionName, data: isRecord(data) ? data : {} }
  } catch {
    try {
      const fallback = await requestJson(runtime.base, '/api/control', {
        method: 'POST',
        body: JSON.stringify({ action: `${actionName} ${Math.max(1, Math.floor(seconds || 0))} ${toStringTrimmed(filename)}` }),
      })
      return { ok: true, action: actionName, data: isRecord(fallback) ? fallback : {} }
    } catch {
      throw makeError('Démarrage enregistrement impossible.')
    }
  }
}

export const stopRecord = async () => {
  const runtime = await getRuntimeInfo()
  const actionName = 'REC_STOP'

  try {
    const data = await requestJson(runtime.base, '/api/media/record/stop', { method: 'POST' })
    return { ok: true, action: actionName, data: isRecord(data) ? data : {} }
  } catch {
    try {
      const fallback = await requestJson(runtime.base, '/api/control', {
        method: 'POST',
        body: JSON.stringify({ action: actionName }),
      })
      return { ok: true, action: actionName, data: isRecord(fallback) ? fallback : {} }
    } catch {
      throw makeError('Stop enregistrement impossible.')
    }
  }
}

export const getMediaRuntimeStatus = async (): Promise<RuntimeMediaState> => {
  const runtime = await getRuntimeInfo()
  try {
    const recordPayload = await requestJson(runtime.base, '/api/media/record/status')
    if (isRecord(recordPayload)) {
      const normalizedFromRecord = normalizeMediaState(recordPayload.media || recordPayload)
      if (normalizedFromRecord.last_error || normalizedFromRecord.ready || normalizedFromRecord.playing || normalizedFromRecord.recording) {
        return normalizedFromRecord
      }
    }
  } catch {
    // fallback
  }

  const statusPayload = await normalizeStatusPayload(runtime.base)
  if (!statusPayload) {
    return {
      ready: false,
      playing: false,
      recording: false,
      record_simulated: true,
      record_limit_seconds: 0,
      record_elapsed_seconds: 0,
      record_file: '',
      last_error: '',
      media_dirs: {
        music_dir: '/music',
        picture_dir: '/picture',
        record_dir: '/recorder',
      },
    }
  }

  const mediaPayload = isRecord(statusPayload.media)
    ? (statusPayload.media as RawRecord)
    : isRecord((statusPayload.story as RawRecord)?.media)
      ? ((statusPayload.story as RawRecord).media as RawRecord)
      : {}

  return normalizeMediaState(mediaPayload)
}

export const getNetworkSnapshot = async (): Promise<RuntimeNetworkState> => {
  const runtime = await getRuntimeInfo()
  const statusPayload = await normalizeStatusPayload(runtime.base)
  if (!statusPayload) {
    return {
      wifi: {},
      espnow: {},
      last_error: 'Aucun statut /api/status disponible.',
    }
  }

  return normalizeNetworkState(statusPayload)
}

export const getAuditTrail = async (limit = 20) => {
  const runtime = await getRuntimeInfo()
  try {
    const payload = await requestJson(runtime.base, `/api/audit/log?limit=${encodeURIComponent(String(limit))}`)
    if (!isRecord(payload) || !Array.isArray(payload.events)) {
      return []
    }
    return payload.events.map((entry) => isRecord(entry) ? entry : { raw: entry })
  } catch {
    return []
  }
}

export const getScenarioList = listScenarios
