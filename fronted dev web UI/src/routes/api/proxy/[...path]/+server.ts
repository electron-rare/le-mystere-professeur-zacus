import { error, json, type RequestHandler } from '@sveltejs/kit'

const FORWARDED_HEADERS = new Set([
  'accept',
  'content-type',
  'authorization',
  'x-requested-with',
  'cache-control',
])

const DEFAULT_TIMEOUT_MS = 7000

const forward: RequestHandler = async ({ params, request, url, fetch }) => {
  const deviceBase = (process.env.VITE_API_BASE || '').trim().replace(/\/+$/, '')
  if (!deviceBase) {
    throw error(500, 'VITE_API_BASE manquant pour le proxy API.')
  }

  const routePath = params.path ?? ''
  if (!routePath.startsWith('api/')) {
    return json({ ok: false, error: 'proxy_path_must_start_with_api' }, { status: 400 })
  }

  const targetUrl = `${deviceBase}/${routePath}${url.search}`
  const headers = new Headers()
  request.headers.forEach((value, key) => {
    if (FORWARDED_HEADERS.has(key.toLowerCase())) {
      headers.set(key, value)
    }
  })

  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), DEFAULT_TIMEOUT_MS)

  try {
    const response = await fetch(targetUrl, {
      method: request.method,
      headers,
      body: request.method === 'GET' || request.method === 'HEAD' ? undefined : await request.text(),
      signal: controller.signal,
    })

    const buffer = await response.arrayBuffer()
    const responseHeaders = new Headers(response.headers)
    responseHeaders.set('x-zacus-proxy', '1')
    return new Response(buffer, {
      status: response.status,
      headers: responseHeaders,
    })
  } catch (err) {
    return json(
      {
        ok: false,
        error: err instanceof Error ? err.message : 'proxy_error',
      },
      { status: 502 },
    )
  } finally {
    clearTimeout(timer)
  }
}

export const GET = forward
export const POST = forward
export const PUT = forward
export const PATCH = forward
export const DELETE = forward
