import { afterEach, describe, expect, it, vi } from 'vitest'
import { requestApi } from '../../src/lib/services/api-client'

afterEach(() => {
  vi.restoreAllMocks()
})

describe('api-client', () => {
  it('retourne ok=false si payload ok=false', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async () =>
        new Response(JSON.stringify({ ok: false, error: 'invalid_kind' }), {
          status: 200,
          headers: { 'content-type': 'application/json' },
        }),
      ) as unknown as typeof fetch,
    )

    const result = await requestApi({
      action: 'MEDIA_FILES',
      base: 'http://device.local',
      path: '/api/media/files?kind=video',
    })

    expect(result.ok).toBe(false)
    if (!result.ok) {
      expect(result.error).toContain('invalid_kind')
    }
  })

  it('retourne status=0 sur erreur transport', async () => {
    vi.stubGlobal(
      'fetch',
      vi.fn(async () => {
        throw new Error('network down')
      }) as unknown as typeof fetch,
    )

    const result = await requestApi({
      action: 'STATUS',
      base: 'http://device.local',
      path: '/api/status',
    })

    expect(result.ok).toBe(false)
    if (!result.ok) {
      expect(result.status).toBe(0)
    }
  })
})
