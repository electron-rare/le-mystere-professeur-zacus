import { expect, test } from '@playwright/test'

const uploadedYaml = `id: UPLOADED_STORY
version: 2
initial_step: STEP_A
app_bindings:
  - id: APP_AUDIO
    app: AUDIO_PACK
    config:
      volume_pct: 70
      ducking: true
      output: SPEAKER
steps:
  - step_id: STEP_A
    apps: [APP_AUDIO]
    transitions:
      - event_name: BTN_NEXT
        event_type: action
        trigger: on_event
        target_step_id: STEP_B
  - step_id: STEP_B
    apps: [APP_AUDIO]
    transitions: []
`

test('@mock story designer imports yaml, edits bindings and supports undo/redo', async ({ page }) => {
  await page.addInitScript((yamlText: string) => {
    window.localStorage.clear()
    window.localStorage.setItem('story-draft', yamlText)
    window.localStorage.removeItem('story-graph-document')
  }, uploadedYaml)

  await page.route('**/api/story/list', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ scenarios: [{ id: 'DEFAULT', estimated_duration_s: 120 }] }),
    })
  })

  await page.goto('/')
  await page.getByRole('button', { name: 'Designer' }).click()

  await expect(page.getByText("Import YAML se fait depuis l'éditeur YAML")).toBeVisible()
  await page.getByRole('button', { name: 'Import YAML → Graphe' }).click()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await expect(page.getByText('Nodes: 2')).toBeVisible()
  await page.getByRole('button', { name: 'Ajouter node' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  await page.getByRole('button', { name: 'Import YAML → Graphe' }).click()
  await expect(page.getByText('YAML importé dans le graphe.')).toBeVisible()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await page.getByRole('button', { name: 'Ajouter node' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  await page.getByRole('button', { name: 'Annuler', exact: true }).click()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await page.getByRole('button', { name: 'Retablir' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  const scenarioIdInput = page.locator('#scenario-id')
  await scenarioIdInput.fill('APP_TEST_STORY')

  await page.getByRole('button', { name: 'Export Graphe → YAML' }).click()
  await scenarioIdInput.fill('APP_TEMP_STORY')
  await page.getByRole('button', { name: 'Import YAML → Graphe' }).click()
  await expect(scenarioIdInput).toHaveValue('APP_TEST_STORY')
})

test('@mock story designer supports right click node linking and pane actions', async ({ page }) => {
  await page.addInitScript((yamlText: string) => {
    window.localStorage.clear()
    window.localStorage.setItem('story-draft', yamlText)
    window.localStorage.removeItem('story-graph-document')
  }, uploadedYaml)

  await page.route('**/api/story/list', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ scenarios: [{ id: 'DEFAULT', estimated_duration_s: 120 }] }),
    })
  })

  await page.goto('/')
  await page.getByRole('button', { name: 'Designer' }).click()
  await page.getByRole('button', { name: 'Import YAML → Graphe' }).click()
  await expect(page.getByText('Nodes: 2')).toBeVisible()
  await expect(page.getByText('Liens: 1')).toBeVisible()

  await page.locator('.react-flow__node:has-text("STEP_B")').first().click({ button: 'right' })
  await expect(page.getByTestId('story-context-menu')).toBeVisible()
  await page.getByTestId('story-context-node-link').click()
  await page.locator('.react-flow__node:has-text("STEP_A")').first().click()
  await expect(page.getByText('Liens: 2')).toBeVisible()

  await page.locator('.react-flow__pane').dispatchEvent('contextmenu', {
    button: 2,
    bubbles: true,
    cancelable: true,
    clientX: 260,
    clientY: 240,
  })
  await expect(page.getByTestId('story-context-menu')).toBeVisible()
  await page.getByTestId('story-context-canvas-add-node').click({ force: true })
  await expect(page.getByText('Nodes: 3')).toBeVisible()
})
