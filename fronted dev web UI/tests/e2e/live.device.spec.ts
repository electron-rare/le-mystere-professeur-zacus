import { expect, test, type APIResponse } from '@playwright/test'

const LIVE_BASE = process.env.LIVE_DEVICE_BASE ?? 'http://192.168.0.91'

const parseJsonSafe = async (response: APIResponse) => {
  try {
    return (await response.json()) as Record<string, unknown>
  } catch {
    return {}
  }
}

test.afterAll(async ({ request }) => {
  await request.post(`${LIVE_BASE}/api/network/espnow/on`)
})

test('@live reads status and stream endpoints', async ({ request }) => {
  const statusResponse = await request.get(`${LIVE_BASE}/api/status`)
  expect(statusResponse.ok()).toBeTruthy()
  const statusPayload = await parseJsonSafe(statusResponse)
  expect(statusPayload).toHaveProperty('story')

  const streamResponse = await request.get(`${LIVE_BASE}/api/stream`)
  expect(streamResponse.ok()).toBeTruthy()
  expect(streamResponse.headers()['content-type']).toContain('text/event-stream')
  const streamBody = await streamResponse.text()
  expect(streamBody).toContain('event: status')
})

test('@live runs full-control mutations and keeps device coherent', async ({ request }) => {
  const unlock = await request.post(`${LIVE_BASE}/api/scenario/unlock`)
  expect(unlock.ok()).toBeTruthy()

  const next = await request.post(`${LIVE_BASE}/api/scenario/next`)
  expect(next.ok()).toBeTruthy()

  const reconnect = await request.post(`${LIVE_BASE}/api/network/wifi/reconnect`)
  expect(reconnect.status()).toBeLessThan(500)

  const espNowOff = await request.post(`${LIVE_BASE}/api/network/espnow/off`)
  expect(espNowOff.ok()).toBeTruthy()

  const espNowOn = await request.post(`${LIVE_BASE}/api/network/espnow/on`)
  expect(espNowOn.ok()).toBeTruthy()

  const finalStatusResponse = await request.get(`${LIVE_BASE}/api/status`)
  expect(finalStatusResponse.ok()).toBeTruthy()
  const finalStatus = await parseJsonSafe(finalStatusResponse)
  expect(finalStatus).toHaveProperty('espnow')
})
