import { expect, test } from '@playwright/test'

test('@mock detects story_v2 and opens orchestrator', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())

  await page.route('**/api/story/list', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({
        scenarios: [{ id: 'DEFAULT', estimated_duration_s: 120, description: 'Default scenario' }],
      }),
    })
  })
  await page.route('**/api/story/select/**', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ selected: 'DEFAULT', status: 'ready' }),
    })
  })
  await page.route('**/api/story/start', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ status: 'running', current_step: 'STEP_WAIT_UNLOCK' }),
    })
  })
  await page.route('**/api/story/pause', async (route) => {
    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ status: 'paused' }) })
  })
  await page.route('**/api/story/resume', async (route) => {
    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ status: 'running' }) })
  })
  await page.route('**/api/story/skip', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ previous_step: 'STEP_A', current_step: 'STEP_B' }),
    })
  })

  await page.goto('/')

  await expect(page.getByText('Story V2 API')).toBeVisible()
  await expect(page.getByRole('button', { name: 'Play' }).first()).toBeVisible()
  await page.getByRole('button', { name: 'Play' }).first().click()
  await expect(page.getByRole('heading', { name: 'Live Orchestrator' })).toBeVisible()
})

test('@mock maps API 404 and 507 errors to friendly messages', async ({ page }) => {
  await page.addInitScript(() => window.localStorage.clear())

  await page.route('**/api/story/list', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({
        scenarios: [{ id: 'DEFAULT', estimated_duration_s: 120, description: 'Default scenario' }],
      }),
    })
  })
  await page.route('**/api/story/select/**', async (route) => {
    await route.fulfill({
      status: 404,
      contentType: 'application/json',
      body: JSON.stringify({ message: 'not found' }),
    })
  })
  await page.route('**/api/story/deploy', async (route) => {
    await route.fulfill({
      status: 507,
      contentType: 'application/json',
      body: JSON.stringify({ message: 'storage full' }),
    })
  })

  await page.goto('/')

  await page.getByRole('button', { name: 'Play' }).first().click()
  await expect(page.getByText('Scenario not found. Browse available scenarios.')).toBeVisible()

  await page.getByRole('button', { name: 'Story Designer' }).click()
  await page.getByRole('button', { name: 'Deploy' }).click()
  await expect(page.getByText('Device storage full. Delete old scenarios.')).toBeVisible()
})

