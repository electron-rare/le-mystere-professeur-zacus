import { expect, test } from '@playwright/test'

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
