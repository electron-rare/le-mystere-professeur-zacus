import { defineConfig, devices } from '@playwright/test'

const isCi = !!process.env.CI
const hasCliGrep = process.argv.some((arg) => arg === '--grep' || arg.startsWith('--grep='))

export default defineConfig({
  testDir: './tests/e2e',
  timeout: 45_000,
  expect: { timeout: 10_000 },
  fullyParallel: true,
  forbidOnly: isCi,
  retries: isCi ? 1 : 0,
  reporter: 'list',
  grep: hasCliGrep ? undefined : /@mock/,
  use: {
    baseURL: 'http://127.0.0.1:4173',
    headless: true,
    trace: 'retain-on-failure',
  },
  webServer: {
    command:
      'VITE_API_BASE=http://127.0.0.1:4173 VITE_API_PROBE_PORTS=4173 VITE_API_FLAVOR=auto npm run dev -- --host 127.0.0.1 --port 4173',
    url: 'http://127.0.0.1:4173',
    reuseExistingServer: !isCi,
    timeout: 120_000,
  },
  projects: [
    {
      name: 'chromium',
      use: { ...devices['Desktop Chrome'] },
    },
  ],
})
