import { expect, test } from '@playwright/test'

const uploadedYaml = `id: UPLOADED_STORY
version: 2
initial_step: STEP_A
app_bindings:
  - id: APP_AUDIO
    app: AUDIO_PACK
steps:
  - step_id: STEP_A
    screen_scene_id: SCENE_A
    apps: [APP_AUDIO]
    transitions:
      - event_name: BTN_NEXT
        event_type: action
        trigger: on_event
        target_step_id: STEP_B
  - step_id: STEP_B
    screen_scene_id: SCENE_B
    apps: [APP_AUDIO]
    transitions: []
`

test('@mock story designer imports yaml and supports undo/redo', async ({ page }) => {
  await page.addInitScript((yamlText: string) => {
    window.localStorage.clear()
    window.localStorage.setItem('studio:v3:draft', yamlText)
    window.localStorage.removeItem('studio:v3:graph')
  }, uploadedYaml)

  await page.route('**/api/**', async (route) => {
    const url = new URL(route.request().url())
    if (url.pathname === '/api/story/list') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ scenarios: [{ id: 'DEFAULT', estimated_duration_s: 120 }] }),
      })
      return
    }

    if (url.pathname === '/api/story/status') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          status: 'idle',
          scenario_id: 'DEFAULT',
          current_step: 'STEP_A',
          progress_pct: 0,
          started_at_ms: 0,
          selected: 'DEFAULT',
          queue_depth: 0,
        }),
      })
      return
    }

    if (url.pathname === '/api/status') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({
          story: { scenario: 'DEFAULT', step: 'STEP_A', screen: 'SCENE_A' },
          media: {},
          network: {},
        }),
      })
      return
    }

    await route.fulfill({ status: 200, contentType: 'application/json', body: JSON.stringify({ ok: true }) })
  })

  await page.goto('/')
  await page.getByRole('link', { name: 'Designer' }).click()

  await page.getByRole('button', { name: 'Import YAML → Graphe' }).click()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await page.getByRole('button', { name: 'Ajouter node' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  await page.getByRole('button', { name: 'Annuler', exact: true }).click()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await page.getByRole('button', { name: 'Rétablir' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  await page.locator('#scenario-id').fill('APP_TEST_STORY')
  await page.getByRole('button', { name: 'Export Graphe → YAML' }).click()
  await expect(page.locator('#designer-yaml')).toHaveValue(/id: APP_TEST_STORY/)
})
