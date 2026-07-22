<template>
  <div class="page-shell system-overview-page">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="cpu" /> {{ t('systemOverview.eyebrow') }}</span>
        <h1 class="hero-title">{{ t('systemOverview.title') }}</h1>
        <p class="hero-subtitle">{{ t('systemOverview.subtitle') }}</p>
      </div>
      <div class="hero-meta">
        <span class="meta-chip"><AppIcon name="firmware" /> {{ data.idfVersion || '—' }}</span>
        <span class="meta-chip"><AppIcon name="board" /> {{ data.target || 'ESP32' }}</span>
        <span class="meta-chip" :class="data.webui?.valid ? 'success-chip' : 'warning-chip'">
          <AppIcon :name="data.webui?.valid ? 'check' : 'alert'" />
          {{ data.webui?.source || 'embedded' }}
        </span>
      </div>
    </section>

    <div v-if="loading" class="overview-grid">
      <div v-for="index in 6" :key="index" class="overview-card skeleton"></div>
    </div>

    <template v-else>
      <BAlert v-if="error" variant="danger" :model-value="true">{{ error }}</BAlert>

      <section class="overview-grid">
        <article class="overview-card">
          <span class="overview-label">{{ t('systemOverview.freeRam') }}</span>
          <strong>{{ formatBytes(data.freeInternalHeap) }}</strong>
          <small>{{ t('systemOverview.currentAvailable') }}</small>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ t('systemOverview.usedRam') }}</span>
          <strong>{{ formatBytes(data.usedInternalHeap) }}</strong>
          <small>{{ formatPercent(data.internalHeapUsagePercent) }}</small>
          <div class="usage-track"><span :style="{ width: usageWidth + '%' }"></span></div>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ t('systemOverview.totalRam') }}</span>
          <strong>{{ formatBytes(data.totalInternalHeap) }}</strong>
          <small>{{ t('systemOverview.allocatableRam') }}</small>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ t('systemOverview.minimumHeap') }}</span>
          <strong>{{ formatBytes(data.minimumFreeHeap) }}</strong>
          <small>{{ t('systemOverview.sinceBoot') }}</small>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ t('systemOverview.largestBlock') }}</span>
          <strong>{{ formatBytes(data.largestFreeBlock) }}</strong>
          <small>{{ t('systemOverview.contiguous') }}</small>
        </article>
        <article class="overview-card">
          <span class="overview-label">{{ t('systemOverview.flash') }}</span>
          <strong>{{ formatBytes(data.flashBytes) }}</strong>
          <small>{{ t('systemOverview.physicalFlash') }}</small>
        </article>
      </section>

      <section class="detail-grid">
        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge soft"><AppIcon name="cpu" /></span>
            <div><h2>{{ t('systemOverview.runtime') }}</h2><p>{{ t('systemOverview.runtimeHint') }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ t('systemOverview.idf') }}</span><strong class="mono">{{ data.idfVersion || '—' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.target') }}</span><strong>{{ data.target || '—' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.cores') }}</span><strong>{{ data.chipCores ?? '—' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.chipRevision') }}</span><strong>{{ data.chipRevision ?? '—' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.resetReason') }}</span><strong>{{ data.resetReasonText || data.resetReason || '—' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.psram') }}</span><strong>{{ data.psramAvailable ? t('systemOverview.available') : t('systemOverview.notAvailable') }}</strong></div>
            <div v-if="data.psramAvailable" class="kv-row"><span>{{ t('systemOverview.psramTotal') }}</span><strong>{{ formatBytes(data.totalPsram) }}</strong></div>
            <div v-if="data.psramAvailable" class="kv-row"><span>{{ t('systemOverview.psramFree') }}</span><strong>{{ formatBytes(data.freePsram) }}</strong></div>
          </div>
        </article>

        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge warning"><AppIcon name="firmware" /></span>
            <div><h2>{{ t('systemOverview.partitions') }}</h2><p>{{ t('systemOverview.partitionsHint') }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ t('systemOverview.runningPartition') }}</span><strong class="mono">{{ data.runningPartition || '—' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.runningSize') }}</span><strong>{{ formatBytes(data.runningPartitionSize) }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.nextPartition') }}</span><strong class="mono">{{ data.nextUpdatePartition || '—' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.nextSize') }}</span><strong>{{ formatBytes(data.nextUpdatePartitionSize) }}</strong></div>
          </div>
        </article>

        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge info"><AppIcon name="globe" /></span>
            <div><h2>WebUI</h2><p>{{ t('systemOverview.webuiHint') }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ t('systemOverview.source') }}</span><strong>{{ data.webui?.source || 'embedded' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.version') }}</span><strong class="mono">{{ data.webui?.version || 'embedded' }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.partitionSize') }}</span><strong>{{ formatBytes(data.webui?.partitionBytes) }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.used') }}</span><strong>{{ formatBytes(data.webui?.usedBytes) }}</strong></div>
          </div>
        </article>

        <article class="detail-card">
          <div class="detail-heading">
            <span class="icon-badge success"><AppIcon name="logs" /></span>
            <div><h2>{{ t('systemOverview.logs') }}</h2><p>{{ t('systemOverview.logsHint') }}</p></div>
          </div>
          <div class="kv-list">
            <div class="kv-row"><span>{{ t('systemOverview.capture') }}</span><strong>{{ data.logs?.enabled ? t('systemOverview.active') : t('systemOverview.inactive') }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.bufferSize') }}</span><strong>{{ formatBytes(data.logs?.bufferBytes) }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.availableLogs') }}</span><strong>{{ formatBytes(data.logs?.availableBytes) }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.totalStream') }}</span><strong>{{ formatBytes(data.logs?.totalWritten) }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.subscribers') }}</span><strong>{{ data.logs?.subscribers ?? 0 }}</strong></div>
            <div class="kv-row"><span>{{ t('systemOverview.crashTail') }}</span><strong :class="data.logs?.crashTailAvailable ? 'warning-text' : ''">{{ data.logs?.crashTailAvailable ? t('systemOverview.available') : t('systemOverview.notAvailable') }}</strong></div>
          </div>
        </article>
      </section>

      <BAlert variant="info" :model-value="true">
        {{ t('systemOverview.onDemandHint') }} <code>/recovery</code>
      </BAlert>

      <div class="action-row">
        <BButton variant="outline-primary" :disabled="loading" @click="loadOverview">
          <AppIcon name="refresh" /> {{ t('systemOverview.refresh') }}
        </BButton>
      </div>
    </template>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import axios from 'axios'

const { t } = useI18n()
const loading = ref(true)
const error = ref('')
const data = ref({ webui: {}, logs: {} })

const usageWidth = computed(() => Math.min(100, Math.max(0, Number(data.value.internalHeapUsagePercent) || 0)))

const formatBytes = (value) => {
  if (value === null || value === undefined || Number.isNaN(Number(value))) return '—'
  const bytes = Number(value)
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`
}

const formatPercent = (value) => `${(Number(value) || 0).toFixed(1)} %`

const loadOverview = async () => {
  loading.value = true
  error.value = ''
  try {
    const response = await axios.get('/api/system/overview', { timeout: 8000 })
    data.value = response.data || { webui: {}, logs: {} }
  } catch (requestError) {
    const responseData = requestError.response?.data
    error.value = responseData?.error
      || (typeof responseData === 'string' ? responseData : '')
      || requestError.message
      || 'System overview unavailable'
  } finally {
    loading.value = false
  }
}

onMounted(loadOverview)
</script>

<style scoped>
.system-overview-page { display: flex; flex-direction: column; gap: var(--section-gap); }
.success-chip { color: var(--color-success); }
.warning-chip, .warning-text { color: var(--color-warning); }
.overview-grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: var(--space-4); }
.overview-card, .detail-card { border: 1px solid var(--color-border-light); background: var(--color-surface); box-shadow: var(--shadow-md); }
.overview-card { min-height: 135px; border-radius: var(--radius-lg); padding: var(--card-padding); display: flex; flex-direction: column; gap: var(--space-2); }
.overview-card strong { font-size: var(--fs-2xl); font-weight: var(--font-weight-bold); }
.overview-card small, .overview-label { color: var(--color-text-secondary); }
.overview-label { font-size: var(--fs-2xs); text-transform: uppercase; letter-spacing: .08em; font-weight: var(--font-weight-heavy); }
.usage-track { height: 6px; border-radius: var(--radius-full); background: var(--color-border-light); overflow: hidden; margin-top: auto; }
.usage-track span { display: block; height: 100%; background: var(--color-primary); }
.detail-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: var(--space-4); }
.detail-card { border-radius: var(--radius-xl); padding: var(--card-padding); min-width: 0; }
.detail-heading { display: flex; gap: var(--space-3); align-items: flex-start; margin-bottom: 18px; }
.detail-heading h2 { margin: 0; font-size: var(--fs-lg); font-weight: var(--font-weight-semibold); }
.detail-heading p { margin: 3px 0 0; color: var(--color-text-secondary); font-size: var(--fs-xs); }
.kv-list { display: flex; flex-direction: column; }
.kv-row { display: flex; justify-content: space-between; gap: 14px; padding: 11px 0; border-bottom: 1px solid var(--color-border-light); }
.kv-row:last-child { border-bottom: 0; }
.kv-row span { color: var(--color-text-secondary); }
.kv-row strong { text-align: right; overflow-wrap: anywhere; font-weight: var(--font-weight-medium); }
.mono { font-family: var(--font-mono); }
.action-row { display: flex; justify-content: flex-end; }
.skeleton { min-height: 135px; animation: pulse 1.4s ease-in-out infinite; }
@keyframes pulse { 0%,100% { opacity: .45; } 50% { opacity: .8; } }
@media (max-width: 1000px) { .overview-grid, .detail-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
@media (max-width: 700px) { .overview-grid, .detail-grid { grid-template-columns: 1fr; } }
@media (max-width: 560px) { .kv-row { flex-direction: column; gap: var(--space-1); } .kv-row strong { text-align: left; } }
</style>
