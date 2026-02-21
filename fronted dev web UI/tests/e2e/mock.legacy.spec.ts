import { expect, test } from '@playwright/test'

const legacyStatusPayload = {
  network: { state: 'connected', mode: 'STA' },
  story: { scenario: 'DEFAULT', step: 'STEP_WAIT_UNLOCK' },
  espnow: { ready: true },
}

test('@mock detects freenove_legacy and limits unsupported actions', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())

  let skipCalls = 0

  await page.route('**/api/story/list', async (route) => {
    await route.fulfill({
      status: 404,
      contentType: 'application/json',
      body: JSON.stringify({ ok: false, error: 'not_found' }),
    })
  })
  await page.route('**/api/status', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify(legacyStatusPayload),
    })
  })
  await page.route('**/api/stream', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'text/event-stream',
      body: `event: status\ndata: ${JSON.stringify(legacyStatusPayload)}\n\nevent: done\ndata: 1\n\n`,
    })
  })
  await page.route('**/api/scenario/next', async (route) => {
    skipCalls += 1
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ action: 'NEXT', ok: true }),
    })
  })

  await page.goto('/')

  await expect(page.getByText('Freenove Legacy API')).toBeVisible()
  await expect(
    page.getByText('Legacy mode detected. Scenario selection and start are unavailable, but you can monitor'),
  ).toBeVisible()

  await page.getByRole('button', { name: 'Open monitor' }).first().click()
  await expect(page.getByRole('heading', { name: 'Live Orchestrator' })).toBeVisible()
  await expect(page.getByRole('button', { name: 'Pause' })).toBeDisabled()

  await page.getByRole('button', { name: 'Skip' }).click()
  await expect.poll(() => skipCalls).toBeGreaterThan(0)
})

test('@mock keeps Story Designer in read/edit mode for legacy APIs', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())

  await page.route('**/api/story/list', async (route) => {
    await route.fulfill({
      status: 404,
      contentType: 'application/json',
      body: JSON.stringify({ ok: false, error: 'not_found' }),
    })
  })
  await page.route('**/api/status', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify(legacyStatusPayload),
    })
  })

  await page.goto('/')
  await page.getByRole('button', { name: 'Story Designer' }).click()

  await expect(
    page.getByText('Story Designer is in read/edit mode. Validate/deploy actions require Story V2 API support.'),
  ).toBeVisible()
  await expect(page.getByRole('button', { name: 'Validate' })).toBeDisabled()
  await expect(page.getByRole('button', { name: 'Deploy' })).toBeDisabled()
})

