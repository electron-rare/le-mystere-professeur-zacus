export type ApiResult<T> =
  | {
      ok: true
      action: string
      data: T
      status: number
      via: 'direct' | 'proxy'
    }
  | {
      ok: false
      action: string
      error: string
      status: number
      raw?: unknown
      via: 'direct' | 'proxy'
    }

type ApiAccessMode = 'hybrid' | 'direct' | 'proxy'

type ApiRequest = {
  action: string
  base: string
  path: string
  method?: string
  body?: unknown
  headers?: Record<string, string>
  timeoutMs?: number
}

const DEFAULT_TIMEOUT_MS = 6500
const ACCESS_MODE = ((import.meta.env.VITE_API_ACCESS_MODE as string | undefined) ?? 'hybrid').toLowerCase() as ApiAccessMode

const toErrorMessage = (payload: unknown, fallback: string) => {
  if (!payload || typeof payload !== 'object') {
    return fallback
  }

  const record = payload as Record<string, unknown>
  if (typeof record.error === 'string' && record.error.trim()) {
    return record.error.trim()
  }

  if (record.error && typeof record.error === 'object') {
    const nested = record.error as Record<string, unknown>
    const nestedMessage =
      (typeof nested.message === 'string' && nested.message.trim()) ||
      (typeof nested.detail === 'string' && nested.detail.trim()) ||
      (typeof nested.details === 'string' && nested.details.trim())
    if (nestedMessage) {
      return nestedMessage
    }
  }

  if (typeof record.message === 'string' && record.message.trim()) {
    return record.message.trim()
  }

  if (typeof record.detail === 'string' && record.detail.trim()) {
    return record.detail.trim()
  }

  return fallback
}

const parseResponseBody = async (response: Response): Promise<unknown> => {
  const text = await response.text()
  if (!text) {
    return {}
  }

  try {
    return JSON.parse(text)
  } catch {
    return text
  }
}

const requestWithTimeout = async (url: string, init: RequestInit, timeoutMs: number) => {
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), timeoutMs)

  try {
    return await fetch(url, {
      ...init,
      signal: controller.signal,
    })
  } finally {
    clearTimeout(timer)
  }
}

const buildDirectUrl = (base: string, path: string) => {
  const normalizedBase = base.replace(/\/+$/, '')
  return `${normalizedBase}${path}`
}

const buildProxyUrl = (path: string) => {
  const normalizedPath = path.startsWith('/') ? path.slice(1) : path
  return `/api/proxy/${normalizedPath}`
}

const runRequest = async <T>(request: ApiRequest, via: 'direct' | 'proxy'): Promise<ApiResult<T>> => {
  const method = request.method ?? 'GET'
  const hasBody = request.body !== undefined && request.body !== null
  const headers: Record<string, string> = {
    Accept: 'application/json',
    ...(hasBody ? { 'Content-Type': 'application/json' } : {}),
    ...(request.headers ?? {}),
  }

  const targetUrl = via === 'proxy' ? buildProxyUrl(request.path) : buildDirectUrl(request.base, request.path)

  try {
    const response = await requestWithTimeout(
      targetUrl,
      {
        method,
        headers,
        body: hasBody ? JSON.stringify(request.body) : undefined,
      },
      request.timeoutMs ?? DEFAULT_TIMEOUT_MS,
    )

    const payload = await parseResponseBody(response)

    if (!response.ok) {
      return {
        ok: false,
        action: request.action,
        error: toErrorMessage(payload, `${response.status} ${response.statusText || 'Request failed'}`),
        status: response.status,
        raw: payload,
        via,
      }
    }

    if (payload && typeof payload === 'object' && (payload as Record<string, unknown>).ok === false) {
      return {
        ok: false,
        action: request.action,
        error: toErrorMessage(payload, 'Erreur API'),
        status: response.status,
        raw: payload,
        via,
      }
    }

    return {
      ok: true,
      action: request.action,
      status: response.status,
      data: payload as T,
      via,
    }
  } catch (error) {
    return {
      ok: false,
      action: request.action,
      error: error instanceof Error ? error.message : 'Erreur réseau',
      status: 0,
      via,
    }
  }
}

export const requestApi = async <T>(request: ApiRequest): Promise<ApiResult<T>> => {
  if (ACCESS_MODE === 'proxy') {
    return runRequest<T>(request, 'proxy')
  }

  const direct = await runRequest<T>(request, 'direct')
  if (ACCESS_MODE === 'direct') {
    return direct
  }

  if (direct.ok) {
    return direct
  }

  // mode hybride: fallback proxy sur erreur transport/CORS ou erreurs HTTP
  const proxied = await runRequest<T>(request, 'proxy')
  return proxied.ok ? proxied : direct
}

export const unwrapApiResult = <T>(result: ApiResult<T>): T => {
  if (result.ok) {
    return result.data
  }
  throw new Error(result.error)
}
