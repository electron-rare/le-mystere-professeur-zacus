import { expect, test } from '@playwright/test'

const legacyStatusPayload = {
  story: {
    scenario: 'DEFAULT',
    step: 'STEP_WAIT_UNLOCK',
    screen: 'SCENE_WAIT_UNLOCK',
  },
  media: {
    ready: true,
    playing: false,
    recording: false,
    record_simulated: true,
    last_error: '',
  },
  network: {
    wifi: { connected: true, ssid: 'LEGACY_WIFI' },
    espnow: { enabled: true },
  },
}

test('@mock detects freenove_legacy and keeps controls constrained', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())

  let skipCalls = 0

  await page.route('**/api/**', async (route) => {
    const url = new URL(route.request().url())

    if (url.pathname === '/api/story/list') {
      await route.fulfill({ status: 404, contentType: 'application/json', body: JSON.stringify({ ok: false, error: 'not_found' }) })
      return
    }

    if (url.pathname === '/api/status') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify(legacyStatusPayload) })
      return
    }

    if (url.pathname === '/api/stream') {
      await route.fulfill({
        status: 200,
        contentType: 'text/event-stream',
        body: `event: status\ndata: ${JSON.stringify(legacyStatusPayload)}\n\n`,
      })
      return
    }

    if (url.pathname === '/api/scenario/next') {
      skipCalls += 1
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, action: 'NEXT' }) })
      return
    }

    if (url.pathname === '/api/scenario/unlock') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, action: 'UNLOCK' }) })
      return
    }

    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true }) })
  })

  await page.goto('/')

  await expect(page.getByRole('link', { name: 'Dashboard' })).toBeVisible({ timeout: 30000 })
  await page.getByRole('button', { name: 'Lancer' }).click()
  await expect(page.getByText('Orchestrateur live')).toBeVisible()

  await page.getByRole('button', { name: 'Skip' }).click()
  await expect.poll(() => skipCalls).toBeGreaterThan(0)
})

test('@mock keeps Story Designer in read/edit mode for legacy APIs', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())

  await page.route('**/api/**', async (route) => {
    const url = new URL(route.request().url())

    if (url.pathname === '/api/story/list') {
      await route.fulfill({ status: 404, contentType: 'application/json', body: JSON.stringify({ ok: false, error: 'not_found' }) })
      return
    }

    if (url.pathname === '/api/status') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify(legacyStatusPayload) })
      return
    }

    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true }) })
  })

  await page.goto('/')
  await page.getByRole('link', { name: 'Designer' }).click()

  await expect(page.getByText('Mode lecture/edition: validate/deploy requierent les APIs Story V2.')).toBeVisible()
  await expect(page.getByRole('button', { name: 'Valider', exact: true })).toBeDisabled()
  await expect(page.getByRole('button', { name: 'Déployer' })).toBeDisabled()
})
