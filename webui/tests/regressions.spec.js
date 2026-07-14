import { test, expect } from '@playwright/test'

const BASE_URL = 'http://127.0.0.1:1234'

const settings = {
  adminUsername: 'admin',
  hostname: 'hb-rf-eth',
  useDHCP: true,
  localIP: '192.168.1.200',
  netmask: '255.255.255.0',
  gateway: '192.168.1.1',
  dns1: '192.168.1.1',
  dns2: '',
  ccuIP: '192.168.1.10',
  timesource: 0,
  dcfOffset: 0,
  gpsBaudrate: 9600,
  ntpServer: 'pool.ntp.org',
  ledBrightness: 50,
  ledPrograms: {},
  enableIPv6: false,
  ipv6Mode: 'auto',
  ipv6Address: '',
  ipv6PrefixLength: 64,
  ipv6Gateway: '',
  ipv6Dns1: '',
  ipv6Dns2: '',
  flashPause: false
}

const archive = {
  releases: [{
    id: 'v2.2.3',
    version: '2.2.3',
    tagName: 'v2.2.3',
    prerelease: false,
    publishedAt: '2026-07-09T00:00:00Z',
    downloadUrl: 'https://github.com/Xerolux/HB-RF-ETH-ng/releases/download/v2.2.3/firmware_2.2.3.bin'
  }]
}

test.beforeEach(async ({ page }) => {
  await page.addInitScript(() => {
    if (!localStorage.getItem('locale')) localStorage.setItem('locale', 'en')
    sessionStorage.setItem('hb-rf-eth-ng-pw', 'device-secret-token')
    sessionStorage.setItem('hb-rf-eth-ng-last-activity', String(Date.now()))
  })

  await page.route('**/sysinfo.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ sysInfo: { currentVersion: '2.2.4-Beta.3' } })
  }))

  await page.route('**/api/check_update**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ latestVersion: '2.2.4-Beta.3', updateAvailable: false })
  }))
})

test('settings save/discard state follows the edited form immediately', async ({ page }) => {
  const persisted = { ...settings }
  const getUrls = []
  const postUrls = []

  await page.route('**/settings.json**', async route => {
    if (route.request().method() === 'POST') {
      postUrls.push(route.request().url())
      Object.assign(persisted, route.request().postDataJSON())
      await route.fulfill({ contentType: 'application/json', body: JSON.stringify({ success: true }) })
      return
    }
    getUrls.push(route.request().url())
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ settings: persisted })
    })
  })

  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Network' }).click()

  const hostname = page.locator('.form-group', { hasText: 'Hostname' }).locator('input')
  const footer = page.locator('.floating-footer')

  await expect(hostname).toHaveValue('hb-rf-eth')
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeDisabled()
  await expect(footer.locator('.save-btn')).toBeDisabled()

  await hostname.fill('changed-host')
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeEnabled()
  await expect(footer.locator('.save-btn')).toBeEnabled()

  let discardDialogShown = false
  page.once('dialog', dialog => {
    discardDialogShown = true
    dialog.dismiss()
  })
  await footer.locator('.discard-btn').click()
  await expect(hostname).toHaveValue('hb-rf-eth')
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeDisabled()
  await expect(footer.locator('.save-btn')).toBeDisabled()
  expect(discardDialogShown).toBe(false)

  await hostname.fill('saved-host')
  await expect(footer).toBeVisible()
  await footer.locator('.save-btn').click()
  await expect(footer).toBeVisible()
  await expect(footer.locator('.discard-btn')).toBeDisabled()
  await expect(footer.locator('.save-btn')).toBeDisabled()
  expect(persisted.hostname).toBe('saved-host')
  expect(getUrls.length).toBeGreaterThan(0)
  expect(getUrls.every(url => new URL(url).searchParams.has('t'))).toBe(true)
  expect(postUrls).toEqual([`${BASE_URL}/settings.json`])
})

test('late header initialization does not discard settings edits', async ({ page }) => {
  let releaseUpdateCheck
  const updateCheckGate = new Promise(resolve => {
    releaseUpdateCheck = resolve
  })
  let settingsGetCount = 0

  await page.route('**/api/check_update**', async route => {
    await updateCheckGate
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ latestVersion: '2.2.4-Beta.3', updateAvailable: false })
    })
  })
  await page.route('**/settings.json**', async route => {
    if (route.request().method() === 'GET') settingsGetCount++
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ settings })
    })
  })

  const updateResponse = page.waitForResponse(response => response.url().includes('/api/check_update'))
  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Network' }).click()

  const hostname = page.locator('.form-group', { hasText: 'Hostname' }).locator('input')
  await expect(hostname).toHaveValue('hb-rf-eth')
  await hostname.fill('edit-must-survive')
  await expect(page.locator('.floating-footer')).toBeVisible()

  releaseUpdateCheck()
  await updateResponse
  await page.waitForTimeout(150)

  expect(settingsGetCount).toBe(1)
  await expect(hostname).toHaveValue('edit-must-survive')
  await expect(page.locator('.floating-footer')).toBeVisible()
})

for (const scenario of [
  {
    name: 'a firmware-valid hostname longer than 32 characters',
    overrides: { hostname: `hb-rf-eth-${'a'.repeat(30)}` }
  },
  {
    name: 'optional empty fields in static IPv6 mode',
    overrides: {
      enableIPv6: true,
      ipv6Mode: 'static',
      ipv6Address: '2001:db8::10',
      ipv6Gateway: '',
      ipv6Dns1: '',
      ipv6Dns2: ''
    }
  },
  {
    name: 'a firmware-valid IPv4-mapped IPv6 address',
    overrides: {
      enableIPv6: true,
      ipv6Mode: 'static',
      ipv6Address: '::ffff:192.168.1.10',
      ipv6Gateway: '2001:db8::1',
      ipv6Dns1: '2001:4860:4860::8888'
    }
  }
]) {
  test(`settings can still be saved with ${scenario.name}`, async ({ page }) => {
    const persisted = { ...settings, ...scenario.overrides }
    let postCount = 0

    await page.route('**/settings.json**', async route => {
      if (route.request().method() === 'POST') {
        postCount++
        Object.assign(persisted, route.request().postDataJSON())
        await route.fulfill({ contentType: 'application/json', body: JSON.stringify({ success: true }) })
        return
      }
      await route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ settings: persisted })
      })
    })

    await page.goto(`${BASE_URL}/settings`)
    const ccuIp = page.locator('.form-group', { hasText: 'CCU IP address' }).locator('input')
    await ccuIp.fill('192.168.1.11')
    await page.locator('.floating-footer .save-btn').click()

    await expect.poll(() => postCount).toBe(1)
    await expect(page.locator('.floating-footer')).toBeVisible()
    await expect(page.locator('.floating-footer .save-btn')).toBeDisabled()
  })
}

test('invalid settings reveal the affected tab instead of silently ignoring save', async ({ page }) => {
  let postCount = 0
  await page.route('**/settings.json**', async route => {
    if (route.request().method() === 'POST') {
      postCount++
      await route.fulfill({ contentType: 'application/json', body: JSON.stringify({ success: true }) })
      return
    }
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify({ settings })
    })
  })

  await page.goto(`${BASE_URL}/settings`)
  await page.getByRole('button', { name: 'Network' }).click()
  await page.locator('.form-group', { hasText: 'Hostname' }).locator('input').fill('invalid_hostname')
  await page.getByRole('button', { name: 'General' }).click()
  await page.locator('.form-group', { hasText: 'CCU IP address' }).locator('input').fill('192.168.1.11')
  await page.locator('.floating-footer .save-btn').click()

  await expect(page.getByRole('button', { name: 'Network' })).toHaveClass(/active/)
  await expect(page.locator('.floating-footer .footer-alert')).toContainText('Please correct the highlighted fields.')
  expect(postCount).toBe(0)
})

test('OTA success dialog uses the active locale and has no default cancel button', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))

  await page.goto(BASE_URL)
  await page.evaluate(() => {
    localStorage.setItem('locale', 'de')
    localStorage.setItem('otaUpdateVersion', '2.2.4-Beta.5')
  })
  await page.reload()

  const dialog = page.getByRole('dialog')
  await expect(dialog).toContainText('Update erfolgreich')
  await expect(dialog).toContainText('Die Firmware wurde erfolgreich auf Version 2.2.4-Beta.5 aktualisiert.')
  await expect(dialog.getByRole('button', { name: 'Schließen', exact: true })).toHaveCount(1)
  await expect(dialog.getByRole('button', { name: /Cancel|Abbrechen/, exact: true })).toHaveCount(0)
})

test('firmware archive loads only from the manifest embedded in the device', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))

  let localRequests = 0
  let externalRequests = 0
  await page.route('**/api/firmware_archive**', async route => {
    localRequests++
    await route.fulfill({
      contentType: 'application/json',
      body: JSON.stringify(archive)
    })
  })
  await page.route('https://raw.githubusercontent.com/Xerolux/HB-RF-ETH-ng/main/archive.json**', async route => {
    externalRequests++
    await route.abort()
  })

  await page.goto(`${BASE_URL}/firmware`)
  await expect.poll(() => localRequests).toBe(1)
  await page.waitForTimeout(250)
  expect(externalRequests).toBe(0)
  await expect(page.locator('.archive-refresh')).toHaveCount(0)
  await expect(page.locator('.archive-error')).toBeHidden()
})

test('crash log modal shows recovered tail once after a crash', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/log/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ enabled: false, persistent: false })
  }))
  await page.route('**/api/crash_log**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      available: true,
      tail: '[heap_watchdog] low heap: free=18432 largest=16384 uptime=84213s\nI (84213) mqtt: published status'
    })
  }))

  await page.goto(`${BASE_URL}/systemlog`)

  // The crash recovery modal auto-opens and shows the recovered tail.
  const modal = page.locator('.modal.show')
  await expect(modal).toBeVisible({ timeout: 5000 })
  await expect(modal).toContainText('[heap_watchdog] low heap')
  await expect(modal).toContainText('published status')

  // Close it.
  await modal.locator('.btn-primary', { hasText: /close/i }).click()
  await expect(modal).toBeHidden()

  // The backend clears the snapshot after the first read — a reload without
  // re-stubbing must not re-show it. The page falls back to the default
  // (empty) route handler here, which returns a 404-ish body; the modal
  // stays hidden.
  await page.unroute('**/api/crash_log**')
  await page.route('**/api/crash_log**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ available: false, tail: '' })
  }))
  await page.goto(`${BASE_URL}/systemlog`)
  await page.waitForTimeout(400)
  await expect(page.locator('.modal.show')).toHaveCount(0)
})

test('live log waits for the authenticated WebSocket acknowledgement and closes cleanly', async ({ page }) => {
  await page.addInitScript(() => {
    class FakeWebSocket {
      static instances = []

      constructor(url) {
        this.url = url
        this.readyState = 0
        FakeWebSocket.instances.push(this)
        queueMicrotask(() => {
          if (this.readyState !== 0) return
          this.readyState = 1
          this.onopen?.({})
        })
      }

      close() {
        if (this.readyState === 3) return
        this.readyState = 3
        queueMicrotask(() => this.onclose?.({ code: 1000 }))
      }

      emit(data) {
        this.onmessage?.({ data })
      }
    }

    window.WebSocket = FakeWebSocket
    window.__fakeWebSockets = FakeWebSocket.instances
  })

  let logGetCount = 0
  await page.route('**/api/log/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ enabled: true, persistent: true, subscribers: 0 })
  }))
  await page.route(/\/api\/log(?:\?.*)?$/, route => {
    logGetCount++
    return route.fulfill({
      contentType: 'text/plain',
      headers: { 'X-Log-Total': '0' },
      body: ''
    })
  })
  await page.route('**/api/log/disable**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ enabled: false })
  }))
  await page.route('**/api/crash_log**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ available: false, tail: '' })
  }))

  await page.goto(`${BASE_URL}/systemlog`)
  const logSocketCount = () => page.evaluate(() =>
    window.__fakeWebSockets.filter(socket => socket.url.includes('/api/log/stream')).length
  )
  await expect.poll(logSocketCount).toBe(1)
  expect(await page.evaluate(() =>
    window.__fakeWebSockets.find(socket => socket.url.includes('/api/log/stream')).url
  )).toContain(
    '/api/log/stream?token=device-secret-token&offset=0'
  )

  // A transport-level open or arbitrary data must not mark the application
  // stream ready. Only the server's post-handshake acknowledgement does.
  await page.evaluate(() =>
    window.__fakeWebSockets.find(socket => socket.url.includes('/api/log/stream')).emit('I (1) premature: ignored\n')
  )
  await expect(page.locator('.log-container')).not.toContainText('premature')

  await page.evaluate(() => {
    const socket = window.__fakeWebSockets.find(socket => socket.url.includes('/api/log/stream'))
    const backlog = 'I (1) app: snapshot backlog\n'
    const backlogBytes = new TextEncoder().encode(backlog).length
    socket.emit(`stream snapshot ${backlogBytes}\n`)
    socket.emit(`stream backlog ${backlogBytes}\n`)
    socket.emit(backlog)
    socket.emit(`stream connected ${backlogBytes}\n`)

    const live = 'I (2) app: live frame\n'
    const liveEnd = backlogBytes + new TextEncoder().encode(live).length
    socket.emit(`stream data ${liveEnd}\n${live}`)
  })
  await expect(page.locator('.log-container')).toContainText('snapshot backlog')
  await expect(page.locator('.log-container')).toContainText('live frame')
  await expect(page.locator('.log-container')).not.toContainText('stream connected')
  await expect(page.locator('.log-container')).not.toContainText('stream snapshot')

  // Once acknowledged, the offset-aware stream is authoritative. The old
  // setInterval captured the initial false state and kept polling every 5s.
  await page.waitForTimeout(5500)
  expect(logGetCount).toBe(1)

  // Intentional shutdown must not let the socket's late close event create a
  // new reconnect loop.
  await page.locator('.toggle-chip input').uncheck()
  await page.waitForTimeout(2200)
  expect(await logSocketCount()).toBe(1)
})
