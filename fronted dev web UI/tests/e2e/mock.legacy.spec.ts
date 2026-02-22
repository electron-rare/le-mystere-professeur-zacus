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

  await expect(page.getByText('API Legacy')).toBeVisible()
  await expect(
    page.getByText('Mode legacy detecte: selection/start sont indisponibles.'),
  ).toBeVisible()

  await page.getByRole('button', { name: 'Ouvrir monitor' }).first().click()
  await expect(page.getByRole('heading', { name: 'Orchestrateur live' })).toBeVisible()
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
  await page.getByRole('button', { name: 'Designer' }).click()

  await expect(page.getByText('Mode lecture/edition: validate/deploy requierent les APIs Story V2.')).toBeVisible()
  await expect(page.getByRole('button', { name: 'Valider' })).toBeDisabled()
  await expect(page.getByRole('button', { name: 'Déployer' })).toBeDisabled()
})
