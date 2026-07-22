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

test('dashboard typography and summary-card footers stay aligned', async ({ page }) => {
  await page.route('**/sysinfo.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      sysInfo: {
        hostname: 'HB-RF-ETH-TestRoma',
        serial: '2805A5114784',
        currentVersion: '2.2.4-Beta.3',
        memoryUsage: 67,
        cpuUsage: 0,
        uptimeSeconds: 1397,
        boardRevision: 'REV 1.10/1.11 (PUB)',
        resetReason: 'Watchdog Reset (Interrupt Watchdog)',
        ethernetConnected: true,
        ethernetSpeed: 100,
        ethernetDuplex: 'Full',
        radioModuleType: 'RPI-RF-MOD',
        radioModuleSerial: '58A9A71150',
        radioModuleFirmwareVersion: '4.4.22',
        radioModuleBidCosRadioMAC: '0x123456',
        radioModuleHmIPRadioMAC: '0x654321',
        radioModuleSGTIN: '3014F711A0001F98A99AXXXX'
      }
    })
  }))

  await page.goto(`${BASE_URL}/`)
  await expect(page.locator('.dashboard .stats-grid')).toBeVisible()

  const typography = await page.locator('.dashboard').evaluate(element => {
    const bodyStyle = getComputedStyle(document.body)
    const values = [...element.querySelectorAll('.kv-value')].map(value => {
      const style = getComputedStyle(value)
      return { fontFamily: style.fontFamily, fontSize: style.fontSize }
    })
    return {
      bodyFontFamily: bodyStyle.fontFamily,
      bodyFontSize: bodyStyle.fontSize,
      values
    }
  })

  expect(typography.values.length).toBeGreaterThan(0)
  expect(typography.values.every(value => value.fontFamily === typography.bodyFontFamily)).toBe(true)
  expect(typography.values.every(value => value.fontSize === typography.bodyFontSize)).toBe(true)

  const footerBottoms = await page.locator('.dashboard .metric-footer').evaluateAll(footers =>
    footers.map(footer => footer.getBoundingClientRect().bottom)
  )
  expect(footerBottoms).toHaveLength(4)
  expect(Math.max(...footerBottoms) - Math.min(...footerBottoms)).toBeLessThan(1)
})

test('monitoring diagnostic rows use one consistent grey row size', async ({ page }) => {
  await page.route('**/api/monitoring', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      checkmk: { enabled: false, port: 6556 },
      mqtt: { enabled: false },
      prometheus: { enabled: false },
      syslog: { enabled: false },
      notify: { enabled: false, channels: 0 }
    })
  }))

  await page.goto(`${BASE_URL}/monitoring`)
  const rows = page.locator('.diagnostics-panel .diag-row')
  await expect(rows).toHaveCount(5)

  const rowStyles = await rows.evaluateAll(elements => elements.map(element => {
    const style = getComputedStyle(element)
    return {
      height: element.getBoundingClientRect().height,
      paddingTop: style.paddingTop,
      paddingBottom: style.paddingBottom,
      backgroundColor: style.backgroundColor
    }
  }))

  expect(new Set(rowStyles.map(row => row.height)).size).toBe(1)
  expect(new Set(rowStyles.map(row => row.paddingTop)).size).toBe(1)
  expect(new Set(rowStyles.map(row => row.paddingBottom)).size).toBe(1)
  expect(new Set(rowStyles.map(row => row.backgroundColor)).size).toBe(1)
  expect(rowStyles[0].backgroundColor).not.toBe('rgba(0, 0, 0, 0)')
})

test('firmware update page follows the selected language completely', async ({ page }) => {
  await page.goto(`${BASE_URL}/updates/firmware`)

  await expect(page.locator('.firmware-page .hero-title')).toHaveText('Update device firmware')
  await expect(page.locator('.firmware-page')).toContainText('Available device firmware')
  await expect(page.locator('.firmware-page')).toContainText('Install firmware manually')
  await expect(page.locator('.firmware-page')).not.toContainText('Firmware aktualisieren')
  await expect(page.locator('.firmware-page')).not.toContainText('Noch kein Prüfergebnis')

  await page.evaluate(() => localStorage.setItem('locale', 'de'))
  await page.reload()

  await expect(page.locator('.firmware-page .hero-title')).toHaveText('Geräte-Firmware aktualisieren')
  await expect(page.locator('.firmware-page')).toContainText('Verfügbare Geräte-Firmware')
  await expect(page.locator('.firmware-page')).toContainText('Firmware manuell installieren')
})

test('updates sub-navigation uses the shared design and switches child routes', async ({ page }) => {
  await page.route('**/settings.json**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({ settings })
  }))
  await page.route('**/api/firmware_archive**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify(archive)
  }))
  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.6',
      effectiveVersion: '1.0.0-Beta.6',
      partitionSize: 327680,
      usedBytes: 250000
    })
  }))

  await page.goto(`${BASE_URL}/updates/firmware`)

  const navigation = page.locator('.updates-page > .tabs-container')
  const control = navigation.locator('.segmented-control')
  const firmwareTab = control.getByRole('link', { name: /Device firmware/ })
  const webUiTab = control.getByRole('link', { name: /WebUI/ })

  await expect(navigation).toBeVisible()
  await expect(navigation).toHaveCSS('display', 'flex')
  await expect(control).toHaveCSS('display', 'grid')
  await expect(firmwareTab).toHaveClass(/active/)
  await expect(firmwareTab).toHaveCSS('min-height', '68px')
  await expect(firmwareTab).toContainText('ESP32, network and radio')
  await expect(webUiTab).toContainText('Browser-based controls')
  await expect(page.locator('.firmware-page')).toBeVisible()
  await expect(page.locator('.firmware-page .content-grid > .update-card')).toHaveCount(2)
  await expect(page.locator('.firmware-page .update-card > .card-header')).toHaveCount(2)
  await expect(page.locator('.firmware-page .update-card > .card-body')).toHaveCount(2)

  const tabSizes = await control.locator('.segment-btn').evaluateAll(tabs => tabs.map(tab => ({
    width: tab.getBoundingClientRect().width,
    height: tab.getBoundingClientRect().height
  })))
  expect(tabSizes).toHaveLength(2)
  expect(Math.abs(tabSizes[0].width - tabSizes[1].width)).toBeLessThan(1)
  expect(tabSizes.every(tab => tab.height >= 44)).toBe(true)

  await webUiTab.click()
  await expect(page).toHaveURL(`${BASE_URL}/updates/webui`)
  await expect(webUiTab).toHaveClass(/active/)
  await expect(page.locator('.www-page')).toBeVisible()
  await expect(page.locator('.www-page .content-grid > .update-card')).toHaveCount(2)
  await expect(page.locator('.www-page .update-card > .card-header')).toHaveCount(2)
  await expect(page.locator('.www-page .update-card > .card-body')).toHaveCount(2)

  await page.setViewportSize({ width: 390, height: 844 })
  const mobileBounds = await control.evaluate(element => {
    const rect = element.getBoundingClientRect()
    return { left: rect.left, right: rect.right, viewportWidth: window.innerWidth }
  })
  expect(mobileBounds.left).toBeGreaterThanOrEqual(0)
  expect(mobileBounds.right).toBeLessThanOrEqual(mobileBounds.viewportWidth)
})

test('firmware keeps a successful no-update result visible without a release version', async ({ page }) => {
  const settledSnapshot = {
    latestVersion: 'n/a',
    updateAvailable: false,
    fetchedAt: 1784750400000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(settledSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  await expect(page.getByText('No check result yet')).toBeVisible()

  await page.getByRole('button', { name: 'Search for updates now' }).click()

  await expect(page.getByText('Firmware is up to date')).toBeVisible()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
})

test('WebUI keeps a successful no-update result visible without a release version', async ({ page }) => {
  const settledSnapshot = {
    latestVersion: 'n/a',
    updateAvailable: false,
    fetchedAt: 1784750400000,
    fetchInProgress: false,
    error: null,
    webui: {
      version: '',
      design: 'newdesign',
      apiVersion: 1,
      minFirmwareVersion: '2.2.5-Beta.1',
      size: 327680
    }
  }

  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.7',
      effectiveVersion: '1.0.0-Beta.7',
      partitionSize: 327680,
      usedBytes: 292666
    })
  }))
  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(settledSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  await expect(page.getByText('The installed WebUI is up to date.')).toHaveCount(0)

  await page.getByRole('button', { name: 'Search for updates now' }).click()

  await expect(page.getByText('The installed WebUI is up to date.')).toBeVisible()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
})

test('update-search cooldown does not fabricate a successful result', async ({ page }) => {
  const emptySnapshot = {
    latestVersion: 'n/a',
    updateAvailable: false,
    fetchedAt: 0,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: false, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(emptySnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  await page.getByRole('button', { name: 'Search for updates now' }).click()

  await expect(page.locator('.app-toast', { hasText: 'A check is already running or the 60-second cooldown is still active.' })).toBeVisible()
  await expect(page.getByText('No check result yet')).toBeVisible()
  await expect(page.getByText('Firmware is up to date')).toHaveCount(0)
})

test('firmware keeps manual no-update feedback visible when it was already current', async ({ page }) => {
  let postCount = 0
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      postCount += 1
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  await expect(page.getByText('Firmware is up to date')).toBeVisible()
  const lastCheck = page.locator('.hero-meta .meta-chip').nth(1)
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
  expect(postCount).toBe(1)

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Everything is up to date')
  await expect(feedback).toContainText('No newer version is available.')
  await expect(lastCheck).not.toHaveText(beforeCheck || '')
})

test('WebUI keeps manual no-update feedback visible when it was already current', async ({ page }) => {
  let postCount = 0
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null,
    webui: {
      version: '1.0.0-Beta.7',
      design: 'newdesign',
      apiVersion: 1,
      minFirmwareVersion: '2.2.4-Beta.1',
      size: 327680
    }
  }

  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.7',
      effectiveVersion: '1.0.0-Beta.7',
      partitionSize: 327680,
      usedBytes: 292666
    })
  }))
  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      postCount += 1
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: true, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  await expect(page.getByText('The installed WebUI is up to date.')).toBeVisible()
  const lastCheck = page.locator('.status-card').nth(2).locator('strong')
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: 'No newer version is available.' })).toBeVisible()
  expect(postCount).toBe(1)

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Everything is up to date')
  await expect(feedback).toContainText('No newer version is available.')
  await expect(lastCheck).not.toHaveText(beforeCheck || '')
})

test('cooldown feedback stays visible without advancing the successful check time', async ({ page }) => {
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: false, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  const lastCheck = page.locator('.hero-meta .meta-chip').nth(1)
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: '60-second cooldown' })).toBeVisible()

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Just a moment')
  await expect(feedback).toContainText('60-second cooldown')
  await expect(lastCheck).toHaveText(beforeCheck || '')
})

test('WebUI cooldown feedback stays visible without advancing the successful check time', async ({ page }) => {
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null,
    webui: {
      version: '1.0.0-Beta.7',
      design: 'newdesign',
      apiVersion: 1,
      minFirmwareVersion: '2.2.4-Beta.1',
      size: 327680
    }
  }

  await page.route('**/api/webui/status**', route => route.fulfill({
    contentType: 'application/json',
    body: JSON.stringify({
      valid: true,
      version: '1.0.0-Beta.7',
      effectiveVersion: '1.0.0-Beta.7',
      partitionSize: 327680,
      usedBytes: 292666
    })
  }))
  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        contentType: 'application/json',
        body: JSON.stringify({ triggered: false, fetchInProgress: false })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/webui`)
  const lastCheck = page.locator('.status-card').nth(2).locator('strong')
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: '60-second cooldown' })).toBeVisible()

  await page.waitForTimeout(4200)
  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Just a moment')
  await expect(feedback).toContainText('60-second cooldown')
  await expect(lastCheck).toHaveText(beforeCheck || '')
})

test('error feedback stays visible without advancing the successful check time', async ({ page }) => {
  const currentSnapshot = {
    latestVersion: '2.2.4-Beta.3',
    updateAvailable: false,
    fetchedAt: 1700000000000,
    fetchInProgress: false,
    error: null
  }

  await page.route('**/api/check_update**', route => {
    if (route.request().method() === 'POST') {
      return route.fulfill({
        status: 500,
        contentType: 'application/json',
        body: JSON.stringify({ error: 'TLS handshake failed' })
      })
    }
    return route.fulfill({ contentType: 'application/json', body: JSON.stringify(currentSnapshot) })
  })

  await page.goto(`${BASE_URL}/updates/firmware`)
  const lastCheck = page.locator('.hero-meta .meta-chip').nth(1)
  const beforeCheck = await lastCheck.textContent()

  await page.getByRole('button', { name: 'Search for updates now' }).click()
  await expect(page.locator('.app-toast', { hasText: 'TLS handshake failed' }).last()).toBeVisible()

  const feedback = page.getByTestId('manual-check-feedback')
  await expect(feedback).toBeVisible()
  await expect(feedback).toContainText('Check failed')
  await expect(feedback).toContainText('TLS handshake failed')
  await expect(lastCheck).toHaveText(beforeCheck || '')
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

  const hostname = page.locator('label.form-label').filter({ hasText: /^Hostname$/ }).locator('..').locator('input')
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

  const hostname = page.locator('label.form-label').filter({ hasText: /^Hostname$/ }).locator('..').locator('input')
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
  await page.locator('label.form-label').filter({ hasText: /^Hostname$/ }).locator('..').locator('input').fill('invalid_hostname')
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

test('firmware update page does not load the retired release archive', async ({ page }) => {
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

  await page.goto(`${BASE_URL}/updates/firmware`)
  await page.waitForTimeout(250)
  expect(localRequests).toBe(0)
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
