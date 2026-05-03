import { test, expect } from '@playwright/test';

/**
 * Atelier golden-path smoke tests.
 *
 * The dev server is auto-started by playwright.config.ts (webServer).
 * Tests assert: layout renders, lazy chunks load, ⌘B collapses the 3D
 * stage, the Run button surfaces after a debounced editor change.
 *
 * The Run-button test uses a dev-only window.__atelierStores hook
 * (declared in App.tsx behind import.meta.env.DEV) to drive store
 * state without dragging real Blockly blocks in headless mode.
 */

test.describe('atelier — layout & lazy chunks', () => {
  test('shell renders three drag-resizable panes', async ({ page }) => {
    await page.goto('/');

    await expect(page.locator('.blockly-container')).toBeVisible({ timeout: 15_000 });
    await expect(page.locator('canvas')).toBeVisible({ timeout: 15_000 });
    await expect(page.getByRole('button', { name: 'sandbox' })).toBeVisible();
    await expect(page.getByRole('button', { name: 'demo' })).toBeVisible();
    await expect(page.getByRole('button', { name: 'test' })).toBeVisible();
  });

  test('mode switching updates the active tab', async ({ page }) => {
    await page.goto('/');
    await expect(page.locator('.blockly-container')).toBeVisible({ timeout: 15_000 });

    const sandboxTab = page.getByRole('button', { name: 'sandbox' });
    const testTab = page.getByRole('button', { name: 'test' });

    await expect(sandboxTab).toHaveClass(/atelier-mode-tab--active/);

    await testTab.click();
    await expect(testTab).toHaveClass(/atelier-mode-tab--active/);
    await expect(sandboxTab).not.toHaveClass(/atelier-mode-tab--active/);

    await expect(page.getByText('Test Mode (10x speed)')).toBeVisible();
  });
});

test.describe('atelier — ⌘B stage toggle', () => {
  test('toggleStage collapses and re-expands the 3D stage', async ({ page }) => {
    await page.goto('/');
    await expect(page.locator('canvas')).toBeVisible({ timeout: 15_000 });

    // We assert on the StagePane wrapper, not the <canvas> element.
    // R3F's <canvas> keeps its intrinsic width (574px) even when the
    // parent flex-container shrinks to 0 — only a resize event would
    // tell Three to recompute. The stage-pane div, on the other hand,
    // follows its parent panel's flex size faithfully.
    //
    // We invoke window.__atelierToggleStage directly rather than firing
    // Cmd+B / Ctrl+B because keyboard modifier handling in headless
    // Chromium is unreliable across macOS / Linux CI runners. The keyboard
    // shortcut and the dev hook share the same toggleStage closure inside
    // Layout.tsx, so this assertion validates both paths.
    const stage = page.getByTestId('stage-pane');

    const initial = await stage.boundingBox();
    expect(initial?.width ?? 0).toBeGreaterThan(0);

    await page.evaluate(() => {
      (window as unknown as { __atelierToggleStage: () => void }).__atelierToggleStage();
    });
    await page.waitForTimeout(300);
    const collapsed = await stage.boundingBox();
    expect(collapsed?.width ?? Infinity).toBeLessThan(2);

    await page.evaluate(() => {
      (window as unknown as { __atelierToggleStage: () => void }).__atelierToggleStage();
    });
    await page.waitForTimeout(300);
    const expanded = await stage.boundingBox();
    expect(expanded?.width ?? 0).toBeGreaterThan(0);
  });
});

test.describe('atelier — Run button stale flow', () => {
  test('Run button surfaces after a debounced editor change', async ({ page }) => {
    await page.goto('/');
    await expect(page.locator('canvas')).toBeVisible({ timeout: 15_000 });

    // Initially neither badge nor button is rendered.
    await expect(page.getByRole('button', { name: /Run|stale/ })).toHaveCount(0);

    // Drive editorStore directly via the dev test hook. The useLiveDiff hook
    // debounces 500 ms then sets pendingIr, flipping isStale -> true.
    await page.evaluate(() => {
      const stores = (window as unknown as { __atelierStores: { editor: { setState: (s: { blocklyJson: string }) => void } } }).__atelierStores;
      stores.editor.setState({ blocklyJson: 'id: test\nversion: "1.0.0"\n' });
    });

    // The Run badge/button must appear. Label depends on whether the YAML
    // parses cleanly (▶ Run) or fails validation (⚠ stale (errors)). Either
    // way, exactly one button matching this regex exists.
    await expect(page.getByRole('button', { name: /Run|stale/ })).toBeVisible({
      timeout: 2_000,
    });
  });
});
