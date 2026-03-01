import { expect, test, type Page } from '@playwright/test'

const setupStoryV2Api = async (page: Page) => {
  await page.route('**/api/**', async (route) => {
    const url = new URL(route.request().url())
    const { pathname, searchParams } = url
    const method = route.request().method()

    if (pathname === '/api/story/list') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ scenarios: [{ id: 'DEFAULT', estimated_duration_s: 120, description: 'Default scenario' }] }),
      })
      return
    }

    if (pathname === '/api/story/status') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          status: 'running',
          scenario_id: 'DEFAULT',
          current_step: 'STEP_FINAL_WIN',
          progress_pct: 84,
          started_at_ms: 1234,
          selected: 'DEFAULT',
          queue_depth: 0,
        }),
      })
      return
    }

    if (pathname === '/api/status') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          story: {
            scenario: 'DEFAULT',
            step: 'STEP_FINAL_WIN',
            screen: 'SCENE_MEDIA_MANAGER',
          },
          media: {
            ready: true,
            playing: false,
            recording: false,
            record_simulated: true,
            last_error: '',
            record_limit_seconds: 20,
            record_elapsed_seconds: 0,
            record_file: '',
            media_dirs: { music_dir: '/music', picture_dir: '/picture', record_dir: '/recorder' },
          },
          network: {
            wifi: { connected: true, ssid: 'ZACUS_WIFI' },
            espnow: { enabled: true },
          },
        }),
      })
      return
    }

    if (pathname === '/api/story/select/DEFAULT' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ selected: 'DEFAULT', status: 'ready' }) })
      return
    }

    if (pathname === '/api/story/start' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ status: 'running', current_step: 'STEP_FINAL_WIN' }) })
      return
    }

    if (pathname === '/api/story/validate' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ valid: true }) })
      return
    }

    if (pathname === '/api/story/deploy' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ deployed: 'DEFAULT', status: 'ok' }) })
      return
    }

    if (pathname === '/api/media/files') {
      const kind = searchParams.get('kind')
      if (kind === 'music') {
        await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, files: ['/music/a.mp3'] }) })
        return
      }
      if (kind === 'picture') {
        await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, files: ['/picture/a.jpg'] }) })
        return
      }
      if (kind === 'recorder') {
        await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, files: ['/recorder/a.wav'] }) })
        return
      }
      await route.fulfill({ status: 400, contentType: 'application/json', body: JSON.stringify({ ok: false, error: 'invalid_kind' }) })
      return
    }

    if (pathname === '/api/media/play' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, action: 'MEDIA_PLAY' }) })
      return
    }

    if (pathname === '/api/media/stop' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, action: 'MEDIA_STOP' }) })
      return
    }

    if (pathname === '/api/media/record/start' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, action: 'REC_START' }) })
      return
    }

    if (pathname === '/api/media/record/stop' && method === 'POST') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true, action: 'REC_STOP' }) })
      return
    }

    if (pathname === '/api/media/record/status') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          media: {
            ready: true,
            playing: false,
            recording: false,
            record_simulated: true,
            last_error: '',
          },
        }),
      })
      return
    }

    if (pathname.startsWith('/api/stream')) {
      await route.fulfill({
        status: 200,
        contentType: 'text/event-stream',
        body: `event: status\ndata: ${JSON.stringify({ story: { step: 'STEP_FINAL_WIN' } })}\n\n`,
      })
      return
    }

    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true }) })
  })
}

test('@mock story_v2 renders dashboard and media hub mapping', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())
  await setupStoryV2Api(page)

  await page.goto('/')

  await expect(page.getByRole('link', { name: 'Dashboard' })).toBeVisible({ timeout: 30000 })

  await page.getByRole('button', { name: 'Lancer' }).click()
  await expect(page.getByText('Orchestrateur live')).toBeVisible()

  await page.getByRole('link', { name: 'Media Manager' }).click()
  await expect(page.getByText('record_simulated: true').first()).toBeVisible()
})

test('@mock scenario deploy displays API errors to operator', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())

  await page.route('**/api/**', async (route) => {
    const url = new URL(route.request().url())
    if (url.pathname === '/api/story/list') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ scenarios: [{ id: 'DEFAULT' }] }) })
      return
    }
    if (url.pathname === '/api/story/status') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'idle', scenario_id: 'DEFAULT', current_step: 'STEP_A', progress_pct: 0, started_at_ms: 0, selected: 'DEFAULT', queue_depth: 0 }),
      })
      return
    }
    if (url.pathname === '/api/status') {
      await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ story: { scenario: 'DEFAULT', step: 'STEP_A', screen: 'SCENE_A' }, media: {} }) })
      return
    }
    if (url.pathname === '/api/story/deploy') {
      await route.fulfill({ status: 507, contentType: 'application/json', body: JSON.stringify({ message: 'storage full' }) })
      return
    }
    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true }) })
  })

  await page.goto('/')
  await page.getByRole('link', { name: 'Scénario' }).click()
  await page.getByLabel('Éditeur YAML rapide').fill('id: TEST\nversion: 2\nsteps: []')
  await page.getByRole('button', { name: 'Déployer YAML' }).click()

  await expect(page.getByText('storage full').first()).toBeVisible()
})
