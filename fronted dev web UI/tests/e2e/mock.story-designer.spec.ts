import { expect, test } from '@playwright/test'
import { Buffer } from 'node:buffer'

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
  await page.addInitScript(() => window.localStorage.clear())

  await page.route('**/api/story/list', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ scenarios: [{ id: 'DEFAULT', estimated_duration_s: 120 }] }),
    })
  })

  await page.goto('/')
  await page.getByRole('button', { name: 'Designer' }).click()

  await page.locator('input[type="file"][accept*=".yaml"]').setInputFiles({
    name: 'uploaded_story.yaml',
    mimeType: 'text/yaml',
    buffer: Buffer.from(uploadedYaml),
  })
  await expect(page.getByText('Fichier uploaded_story.yaml importé.')).toBeVisible()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await expect(page.getByText('Nodes: 2')).toBeVisible()
  await page.getByRole('button', { name: 'Ajouter node' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  await page.getByRole('button', { name: 'Import YAML → Graphe' }).click()
  await expect(page.getByText('YAML importé dans le graphe.')).toBeVisible()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await page.getByRole('button', { name: 'Ajouter node' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  await page.getByRole('button', { name: 'Annuler' }).click()
  await expect(page.getByText('Nodes: 2')).toBeVisible()

  await page.getByRole('button', { name: 'Retablir' }).click()
  await expect(page.getByText('Nodes: 3')).toBeVisible()

  const firstBindingIdInput = page.locator('label:has-text("ID") input').first()
  await firstBindingIdInput.fill('APP_TEST_BIND')

  await page.getByRole('button', { name: 'Export Graphe → YAML' }).click()
  await firstBindingIdInput.fill('APP_TEMP_BIND')
  await page.getByRole('button', { name: 'Import YAML → Graphe' }).click()
  await expect(firstBindingIdInput).toHaveValue('APP_TEST_BIND')
})
