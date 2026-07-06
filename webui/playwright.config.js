import { defineConfig } from '@playwright/test'

export default defineConfig({
  testDir: './tests',
  webServer: {
    command: 'npm run dev -- --host 127.0.0.1 --port 1234 --strictPort',
    url: 'http://127.0.0.1:1234',
    reuseExistingServer: true,
    timeout: 120000
  }
})
