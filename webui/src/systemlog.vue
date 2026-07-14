<template>
  <div class="page-shell log-page">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow">
          <AppIcon name="logs" />
          {{ t('systemlog.title') }}
        </span>
        <h1 class="hero-title">{{ t('systemlog.title') }}</h1>
        <p class="hero-subtitle">{{ t('systemlog.description') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="activity" /> {{ !logEnabled ? t('systemlog.disabled') : (paused ? t('systemlog.paused') : t('systemlog.live')) }}</span>
        <span class="meta-chip"><AppIcon name="search" /> {{ filteredEntries.length }} {{ t('systemlog.lines') }}</span>
        <span v-if="newEntriesCount > 0" class="meta-chip">{{ newEntriesCount }} {{ t('systemlog.newEntries') }}</span>
      </div>
    </section>

    <div class="settings-card card-glass">
      <div class="card-header">
        <div class="header-content">
          <div class="header-icon bg-info-light text-info"><AppIcon name="logs" /></div>
          <div>
            <h3>{{ t('systemlog.liveLog') }}</h3>
            <p class="section-subtitle">{{ t('systemlog.subtitle') }}</p>
          </div>
        </div>
        <div class="log-actions">
          <label class="toggle-chip">
            <input type="checkbox" :disabled="logToggleBusy" v-model="logEnabled">
            <span>{{ logEnabled ? t('systemlog.enabled') : t('systemlog.disabled') }}</span>
          </label>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="refreshLog">
            <AppIcon name="refresh" />
            {{ t('systemlog.refresh') }}
          </button>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="togglePaused">
            <AppIcon name="activity" />
            {{ paused ? t('systemlog.resume') : t('systemlog.pause') }}
          </button>
          <button class="tool-btn" type="button" :disabled="!logEnabled" @click="toggleAutoScroll">
            <AppIcon name="chevronDown" />
            {{ autoScroll ? t('systemlog.autoScroll') : t('systemlog.manualScroll') }}
          </button>
          <button class="tool-btn" type="button" :disabled="!filteredEntries.length" @click="copyVisibleLog">
            <AppIcon name="copy" />
            {{ t('systemlog.copy') }}
          </button>
          <button class="tool-btn" type="button" @click="clearLog">
            <AppIcon name="close" />
            {{ t('systemlog.clear') }}
          </button>
          <button class="btn btn-primary btn-sm" type="button" @click="downloadLog">
            <AppIcon name="download" />
            {{ t('systemlog.download') }}
          </button>
        </div>
      </div>

      <div class="toolbar">
        <div class="search-field">
          <AppIcon name="search" />
          <input v-model.trim="searchQuery" type="search" :placeholder="t('systemlog.searchPlaceholder')">
        </div>
        <select v-model="levelFilter" class="level-filter">
          <option value="all">{{ t('systemlog.allLevels') }}</option>
          <option value="E">{{ t('systemlog.levelError') }}</option>
          <option value="W">{{ t('systemlog.levelWarning') }}</option>
          <option value="I">{{ t('systemlog.levelInfo') }}</option>
          <option value="D">{{ t('systemlog.levelDebug') }}</option>
        </select>
      </div>

      <div class="card-body p-0">
        <div v-if="logEnabled" ref="logContainer" class="log-container" @scroll="onScroll">
          <div v-if="filteredEntries.length" class="log-list">
            <div
              v-for="(entry, index) in filteredEntries"
              :key="`${index}-${entry.raw}`"
              class="log-line"
              :class="levelClass(entry)"
            >
              <code>{{ entry.raw }}</code>
              <button class="line-copy" type="button" @click="copyLine(entry.raw)">
                <AppIcon name="copy" />
              </button>
            </div>
          </div>
          <div v-else class="log-empty">
            {{ searchQuery || levelFilter !== 'all' ? t('systemlog.noMatches') : t('systemlog.empty') }}
          </div>
        </div>
        <div v-else class="log-disabled">
          <p>{{ t('systemlog.disabledMessage') }}</p>
        </div>
      </div>
    </div>

    <!--
      Crash recovery snapshot. When the device restarted due to a watchdog
      or panic, a tail of the in-memory log was saved to NVS right before
      the restart. The snapshot is delivered once (the backend clears it
      after the first read), so this modal auto-opens exactly once after a
      real crash — making "the log was wiped" debuggable for the first time.
    -->
    <BModal
      v-model="showCrashModal"
      :title="t('systemlog.crashTitle')"
      :ok-title="t('common.close')"
      ok-only
      size="lg"
      scrollable
    >
      <div class="alert alert-warning" role="alert">
        {{ t('systemlog.crashHint') }}
      </div>
      <pre class="crash-tail-pre">{{ crashTail }}</pre>
    </BModal>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, onUnmounted, nextTick, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'
import { useUiStore } from './stores.js'
import { copyToClipboard } from './composables/useClipboard'

const { t } = useI18n()
const uiStore = useUiStore()

const logEntries = ref([])
const logContainer = ref(null)
const logEnabled = ref(false)
const logToggleBusy = ref(false)
const autoScroll = ref(true)
const paused = ref(false)
const offset = ref(0)
const searchQuery = ref('')
const levelFilter = ref('all')
const newEntriesCount = ref(0)
// Last-crash tail recovered from NVS after a watchdog/panic restart.
// Empty when no snapshot was stored (normal reboot, first boot, etc.).
const crashTail = ref('')
const showCrashModal = ref(false)
let pollTimer = null
// Suppresses the logEnabled watcher while we sync the toggle from the backend
// status on mount (so the initial state does not trigger a redundant enable
// POST).
let syncingFromBackend = false

const MAX_LOG_LINES = 2500
const MAX_COPY_LINES = 500

// copyToClipboard is provided by ./composables/useClipboard.js (iOS-Safari-safe).

const getEntryLevel = (line) => {
  if (/\bE\s*\(/.test(line) || /\bERROR\b/i.test(line)) return 'E'
  if (/\bW\s*\(/.test(line) || /\bWARN\b/i.test(line)) return 'W'
  if (/\bI\s*\(/.test(line) || /\bINFO\b/i.test(line)) return 'I'
  if (/\bD\s*\(/.test(line) || /\bDEBUG\b/i.test(line)) return 'D'
  return 'N'
}

const levelClass = (entry) => {
  const level = entry.level
  if (level === 'E') return 'error'
  if (level === 'W') return 'warning'
  if (level === 'I') return 'info'
  return ''
}

const filteredEntries = computed(() => {
  const query = searchQuery.value.toLowerCase()
  const filter = levelFilter.value
  return logEntries.value.filter((entry) => {
    const matchesQuery = !query || entry.lowerRaw.includes(query)
    const matchesLevel = filter === 'all' || entry.level === filter
    return matchesQuery && matchesLevel
  })
})

const appendChunk = (chunk) => {
  const lines = chunk
    .split(/\r?\n/)
    .map((line) => line.trimEnd())
    .filter((line) => line.length > 0)

  if (!lines.length) return

  for (const line of lines) {
    if (logEntries.value.length >= MAX_LOG_LINES) {
      logEntries.value.shift()
    }
    logEntries.value.push({
      raw: line,
      level: getEntryLevel(line),
      lowerRaw: line.toLowerCase()
    })
  }

  if (paused.value || !autoScroll.value) {
    newEntriesCount.value += lines.length
  }
}

const startPolling = () => {
  stopPolling()
  fetchLog()
  // Try to upgrade to a live WebSocket. If it connects, polling is throttled
  // to a slow reconciliation rate (so the persisted ring buffer stays in
  // sync even if the WS drops a frame).
  openLogStream()
  pollTimer = setInterval(() => {
    if (!wsConnected.value) fetchLog()
    else                    fetchLog()  // periodic catch-up even on WS
  }, wsConnected.value ? 30000 : 5000)
}

const stopPolling = () => {
  if (pollTimer) {
    clearInterval(pollTimer)
    pollTimer = null
  }
  closeLogStream()
}

// ---- WebSocket live stream (Phase E) -------------------------------------
// The browser cannot attach Authorization headers to a WebSocket upgrade, so
// the token is passed via ?token=. The backend's log_stream_handler accepts
// either that or the header.
let ws = null
let wsReconnectTimer = null
const wsConnected = ref(false)

const openLogStream = () => {
  closeLogStream()
  try {
    const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const token = sessionStorage.getItem('hb-rf-eth-ng-pw') || ''
    const url = `${proto}//${window.location.host}/api/log/stream?token=${encodeURIComponent(token)}`
    ws = new WebSocket(url)
    ws.binaryType = 'arraybuffer'

    ws.onopen = () => { wsConnected.value = true }
    ws.onclose = () => {
      wsConnected.value = false
      ws = null
      // Reconnect while capture is still enabled.
      if (logEnabled.value && !wsReconnectTimer) {
        wsReconnectTimer = setTimeout(() => {
          wsReconnectTimer = null
          if (logEnabled.value) openLogStream()
        }, 2000)
      }
    }
    ws.onerror = () => { /* close handler will reconnect */ }
    ws.onmessage = (ev) => {
      // ev.data is a string text frame from the device.
      if (typeof ev.data !== 'string') return
      // Split into lines; the device sends one frame per log line, but
      // older firmwares may batch.
      const lines = ev.data.split(/\r?\n/).filter(l => l.length > 0)
      if (lines.length) {
        appendChunk(lines.join('\n'))
        if (autoScroll.value) {
          newEntriesCount.value = 0
          nextTick(() => scrollToBottom())
        }
      }
    }
  } catch (e) {
    console.warn('WebSocket log stream unavailable, falling back to polling', e)
  }
}

const closeLogStream = () => {
  if (wsReconnectTimer) {
    clearTimeout(wsReconnectTimer)
    wsReconnectTimer = null
  }
  if (ws) {
    try { ws.close() } catch (e) {}
    ws = null
  }
  wsConnected.value = false
}

watch(logEnabled, async (enabled) => {
  if (syncingFromBackend) return
  if (enabled) {
    logToggleBusy.value = true
    try {
      const response = await axios.post('/api/log/enable', {}, { timeout: 5000, silent: true })
      if (response.data?.enabled !== true) {
        throw new Error('Device could not allocate the log buffer')
      }
    } catch (e) {
      // Backend could not allocate the buffer - revert the toggle.
      logEnabled.value = false
      uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('systemlog.enableFailed') })
      logToggleBusy.value = false
      return
    }
    logToggleBusy.value = false
    offset.value = 0
    newEntriesCount.value = 0
    logEntries.value = []
    startPolling()
  } else {
    stopPolling()
    try {
      await axios.post('/api/log/disable', {}, { timeout: 5000, silent: true })
    } catch (e) {
      // Non-fatal: polling already stopped; backend keeps its current state.
    }
  }
})

const fetchLog = async () => {
  if (paused.value) return

  try {
    const response = await axios.get('/api/log', {
      params: { offset: offset.value },
      timeout: 5000,
      silent: true
    })
    if (response.data) {
      appendChunk(response.data)
      const totalWritten = parseInt(response.headers['x-log-total'])
      if (!isNaN(totalWritten)) {
        offset.value = totalWritten
      } else {
        // The device-side offset counts bytes - measure the chunk in UTF-8
        // bytes, not JS string characters, or multi-byte log content
        // desyncs the poll window.
        offset.value += new TextEncoder().encode(response.data).length
      }
      if (autoScroll.value) {
        newEntriesCount.value = 0
        await nextTick()
        scrollToBottom()
      }
    }
  } catch (error) {
    console.warn('Log poll failed:', error.response?.status || error.message)
  }
}

const refreshLog = () => {
  offset.value = 0
  newEntriesCount.value = 0
  logEntries.value = []
  fetchLog()
}

const downloadLog = async () => {
  try {
    const response = await axios.get('/api/log/download', {
      responseType: 'blob'
    })
    const url = window.URL.createObjectURL(new Blob([response.data]))
    const link = document.createElement('a')
    link.href = url
    link.setAttribute('download', 'hb-rf-eth-log.txt')
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    setTimeout(() => window.URL.revokeObjectURL(url), 100)
  } catch (error) {
    uiStore.pushToast({
      type: 'error',
      title: t('common.error'),
      message: String(error.response?.status || error.message)
    })
  }
}

const copyVisibleLog = async () => {
  const content = filteredEntries.value.slice(-MAX_COPY_LINES).map(e => e.raw).join('\n')
  if (!content) return
  try {
    await copyToClipboard(content)
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('systemlog.copiedVisible'), duration: 1800 })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('systemlog.copyVisibleFailed') })
  }
}

const copyLine = async (line) => {
  try {
    await copyToClipboard(line)
    uiStore.pushToast({ type: 'success', title: t('common.success'), message: t('systemlog.copiedLine'), duration: 1400 })
  } catch (error) {
    uiStore.pushToast({ type: 'error', title: t('common.error'), message: t('systemlog.copyLineFailed') })
  }
}

const scrollToBottom = () => {
  if (logContainer.value) {
    logContainer.value.scrollTop = logContainer.value.scrollHeight
  }
}

const onScroll = () => {
  if (!logContainer.value) return
  const el = logContainer.value
  const atBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 50
  autoScroll.value = atBottom
  if (atBottom) {
    newEntriesCount.value = 0
  }
}

const toggleAutoScroll = () => {
  autoScroll.value = !autoScroll.value
  if (autoScroll.value) {
    newEntriesCount.value = 0
    scrollToBottom()
  }
}

const togglePaused = () => {
  paused.value = !paused.value
  if (!paused.value) {
    newEntriesCount.value = 0
    fetchLog()
  }
}

const clearLog = () => {
  logEntries.value = []
  newEntriesCount.value = 0
}

onMounted(async () => {
  // Ask the device whether the in-memory log buffer is active. Once enabled,
  // the device persists that choice and restores capture after a reboot.
  try {
    const response = await axios.get('/api/log/status', { silent: true })
    syncingFromBackend = true
    logEnabled.value = !!response.data.enabled
    syncingFromBackend = false
  } catch (e) {
    syncingFromBackend = false
    logEnabled.value = false
  }
  if (logEnabled.value) {
    startPolling()
  }

  // Check whether the device stored a log tail right before the last
  // watchdog/panic restart. The endpoint returns {available, tail} and
  // clears the snapshot after the first successful read, so this only
  // surfaces once after a real crash — not on every page load.
  try {
    const r = await axios.get('/api/crash_log', { silent: true })
    if (r.data && r.data.available && r.data.tail) {
      crashTail.value = r.data.tail
      showCrashModal.value = true
    }
  } catch (e) {
    // ignore — older firmware versions do not expose this endpoint
  }
})

onUnmounted(() => {
  stopPolling()
})
</script>

<style scoped>
.log-page {
}

.settings-card {
  border-radius: var(--radius-xl);
  margin-bottom: var(--spacing-lg);
  overflow: hidden;
  min-width: 0;
}

.card-header {
  padding: var(--spacing-lg);
  background: transparent;
  border: none;
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: var(--spacing-md);
  min-width: 0;
}

.header-content {
  display: flex;
  align-items: center;
  gap: var(--spacing-md);
  min-width: 0;
}

.header-content p {
  margin: 4px 0 0;
  color: var(--color-text-secondary);
  font-size: 0.85rem;
  overflow-wrap: anywhere;
}

.header-icon {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.card-header h3 {
  margin: 0;
  font-size: 1.125rem;
  font-weight: 600;
  overflow-wrap: anywhere;
}

.log-actions {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  flex-wrap: wrap;
  gap: 10px;
  min-width: 0;
}

.toggle-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  border-radius: var(--radius-full);
  background: rgba(255, 255, 255, 0.52);
  border: 1px solid var(--color-border-light);
  max-width: 100%;
  overflow-wrap: anywhere;
}

.tool-btn {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.6);
  border-radius: var(--radius-full);
  padding: 8px 12px;
  color: var(--color-text);
  max-width: 100%;
  white-space: normal;
}

.tool-btn:disabled {
  opacity: 0.55;
}

.toolbar {
  padding: 0 var(--spacing-lg) var(--spacing-md);
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  min-width: 0;
}

.search-field {
  flex: 1 1 280px;
  min-width: min(240px, 100%);
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 12px 14px;
  border-radius: var(--radius-full);
  background: rgba(255, 255, 255, 0.64);
  border: 1px solid var(--color-border-light);
}

.search-field input,
.level-filter {
  border: none;
  background: transparent;
  width: 100%;
  color: var(--color-text);
  outline: none;
}

.level-filter {
  width: auto;
  min-width: 150px;
  padding: 12px 14px;
  border-radius: var(--radius-full);
  border: 1px solid var(--color-border-light);
  background: rgba(255, 255, 255, 0.64);
}

.log-container {
  height: clamp(300px, 55vh, 500px);
  overflow-y: auto;
  background: var(--color-surface);
  border: 1px solid var(--color-border);
  border-radius: var(--radius-lg);
  margin: var(--spacing-sm);
  min-width: 0;
}

.log-list {
  padding: 10px;
}

.log-line {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 10px;
  padding: 8px 10px;
  border-radius: 12px;
  color: var(--color-text);
  min-width: 0;
}

.log-line:hover {
  background: rgba(127, 127, 127, 0.1);
}

.log-line code {
  color: inherit;
  white-space: pre-wrap;
  word-break: break-word;
  overflow-wrap: anywhere;
  font-family: "Cascadia Code", "Courier New", monospace;
  font-size: 0.86rem;
  min-width: 0;
}

.log-line.info {
  color: var(--color-text);
}

.log-line.warning {
  color: var(--color-warning);
}

.log-line.error {
  color: var(--color-danger);
}

.line-copy {
  border: none;
  background: transparent;
  color: var(--color-text-secondary);
  padding: 2px;
  flex: 0 0 auto;
}

.log-empty {
  padding: 24px;
  color: var(--color-text-secondary);
}

.log-disabled {
  height: 200px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: var(--color-bg);
  border: 1px solid var(--color-border-light);
  color: var(--color-text-secondary);
  font-size: 0.9rem;
}

.bg-info-light { background-color: var(--color-info-light); }
.text-info { color: var(--color-info); }

@media (max-width: 768px) {
  .settings-card {
    border-radius: var(--radius-lg);
  }

  .card-header {
    padding: var(--spacing-md);
    flex-direction: column;
    align-items: stretch;
  }

  .toolbar {
    padding: 0 var(--spacing-md) var(--spacing-md);
    flex-direction: column;
  }

  .level-filter {
    width: 100%;
  }

  .log-container {
    height: clamp(250px, 50vh, 400px);
  }

  .log-actions {
    gap: 6px;
  }

  .log-actions > *,
  .tool-btn,
  .toggle-chip {
    width: 100%;
    justify-content: center;
  }

  .tool-btn {
    padding: 6px 10px;
    font-size: 0.8125rem;
  }

  .log-line code {
    font-size: 0.78rem;
  }
}

.crash-tail-pre {
  max-height: 60vh;
  overflow: auto;
  background: var(--color-bg-secondary, #1e1e1e);
  color: var(--color-text-inverse, #e0e0e0);
  padding: 12px;
  border-radius: 6px;
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  font-size: 0.8rem;
  line-height: 1.35;
  white-space: pre-wrap;
  word-break: break-word;
  margin: 0;
}
</style>
