export const API_BASE = import.meta.env.VITE_API_BASE ?? 'http://localhost:8080'

export type ApiError = Error & {
  status?: number
  detail?: string
}

const parseError = async (response: Response): Promise<ApiError> => {
  let detail = ''
  try {
    const body = (await response.json()) as { message?: string }
    detail = body?.message ?? ''
  } catch {
    detail = ''
  }

  const error = new Error(detail || response.statusText) as ApiError
  error.status = response.status
  error.detail = detail
  return error
}

const request = async (path: string, options?: RequestInit) => {
  const response = await fetch(`${API_BASE}${path}`, {
    headers: {
      'Content-Type': 'application/json',
      ...(options?.headers ?? {}),
    },
    ...options,
  })

  if (!response.ok) {
    throw await parseError(response)
  }

  if (response.status === 204) {
    return null
  }

  return response.json()
}

export const getStoryList = async () => request('/api/story/list')

export const selectStory = async (id: string) =>
  request(`/api/story/select/${encodeURIComponent(id)}`, { method: 'POST' })

export const startStory = async () => request('/api/story/start', { method: 'POST' })

export const pauseStory = async () => request('/api/story/pause', { method: 'POST' })

export const resumeStory = async () => request('/api/story/resume', { method: 'POST' })

export const skipStory = async () => request('/api/story/skip', { method: 'POST' })

export const validateStory = async (yaml: string) =>
  request('/api/story/validate', {
    method: 'POST',
    body: JSON.stringify({ yaml }),
  })

export const deployStory = async (yaml: string) =>
  request('/api/story/deploy', {
    method: 'POST',
    body: JSON.stringify({ yaml }),
  })

export const getWsUrl = () => {
  const base = API_BASE.replace(/^http/, 'ws')
  return `${base}/api/story/stream`
}
