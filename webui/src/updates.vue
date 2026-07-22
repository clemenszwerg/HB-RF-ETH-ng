<template>
  <div class="updates-page page-shell">
    <!-- Sub-tab switch between Firmware and WebUI (Korrekturauftrag §6).
         Each child route owns its own page-hero with the relevant version
         info, so this wrapper only renders the segmented control. The active
         tab is derived from the current path so direct URLs (/updates/webui)
         highlight correctly. -->
    <nav class="tabs-container" :aria-label="t('nav.updates')">
      <div class="segmented-control">
        <router-link
          v-for="tab in tabs"
          :key="tab.to"
          :to="tab.to"
          class="segment-btn"
          :class="{ active: isActive(tab) }"
        >
          <span class="segment-icon" aria-hidden="true"><AppIcon :name="tab.icon" /></span>
          <span class="segment-copy">
            <strong class="segment-label">{{ tab.label }}</strong>
            <small>{{ tab.description }}</small>
          </span>
        </router-link>
      </div>
    </nav>

    <router-view />
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute } from 'vue-router'

const { t } = useI18n()
const route = useRoute()

const tabs = computed(() => [
  {
    to: '/updates/firmware',
    icon: 'firmware',
    label: t('updates.firmware'),
    description: t('updates.firmwareDescription')
  },
  {
    to: '/updates/webui',
    icon: 'globe',
    label: t('updates.webui'),
    description: t('updates.webuiDescription')
  }
])

const isActive = (tab) => {
  // Active sub-tab matches the current path prefix (so the firmware tab stays
  // highlighted while a sub-route is active too, if any are added later).
  return route.path === tab.to || route.path.startsWith(tab.to + '/')
}
</script>

<style scoped>
.updates-page {
  gap: 16px;
}

/* The colors and surfaces come from the global New Design tokens. Keep the
   layout here because the Settings control's structural rules are scoped to
   settings.vue and therefore cannot style this route-level navigation. */
.tabs-container {
  display: flex;
  justify-content: center;
  width: 100%;
  min-width: 0;
}

.segmented-control {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: var(--space-1);
  width: min(680px, 100%);
  min-width: 0;
}

.segment-btn {
  min-width: 0;
  min-height: 68px;
  padding: var(--space-2) var(--space-3);
  display: inline-flex;
  align-items: center;
  justify-content: flex-start;
  gap: var(--space-3);
  color: var(--color-text-secondary);
  font-size: var(--fs-sm);
  font-weight: var(--font-weight-semibold);
  line-height: var(--line-height-normal);
  text-align: left;
  text-decoration: none;
  transition: color var(--transition-fast), border-color var(--transition-fast), background var(--transition-fast);
}

.segment-btn:hover {
  color: var(--color-text);
}

.segment-btn:focus-visible {
  outline: 2px solid var(--color-primary);
  outline-offset: 2px;
}

.segment-icon {
  width: 36px;
  height: 36px;
  flex: 0 0 auto;
  display: grid;
  place-items: center;
  border-radius: var(--radius-md);
  color: var(--color-text-secondary);
  background: var(--color-bg-alt);
}

.segment-icon .app-icon {
  width: 20px;
  height: 20px;
}

.segment-btn.active .segment-icon {
  color: var(--color-primary-strong);
  background: var(--color-primary-soft);
}

.segment-copy {
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.segment-label {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.segment-copy small {
  color: var(--color-text-secondary);
  font-size: var(--fs-xs);
  font-weight: var(--font-weight-normal);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

@media (max-width: 480px) {
  .segment-btn {
    padding-inline: var(--space-2);
    justify-content: center;
    gap: var(--space-2);
  }

  .segment-icon {
    width: 32px;
    height: 32px;
  }

  .segment-copy small {
    white-space: normal;
  }
}
</style>
