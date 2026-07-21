<template>
  <div class="change-password-page page-shell">
    <section class="page-hero">
      <div class="hero-copy">
        <span class="hero-eyebrow"><AppIcon name="lock" /> {{ t('changePassword.eyebrow') }}</span>
        <h1 class="hero-title">{{ t('changePassword.title') }}</h1>
        <p class="hero-subtitle">{{ t('changePassword.subtitle') }}</p>
      </div>
    </section>

    <section class="surface-card change-password-card">
      <div class="card-body-content">
        <BAlert show variant="warning" class="warning-alert">
          <span class="alert-icon"><AppIcon name="alert" /></span>
          <div class="alert-content">
            <strong>{{ t('changePassword.warningTitle') }}</strong>
            {{ t('changePassword.warningMessage') }}
          </div>
        </BAlert>

        <div class="password-requirements">
          <span class="req-icon"><AppIcon name="list" /></span>
          <div class="req-content">
            <strong>{{ t('changePassword.requirementsTitle') }}</strong>
            <ul>
              <li>{{ t('changePassword.reqMinLength') }}</li>
              <li>{{ t('changePassword.reqLettersNumbers') }}</li>
            </ul>
          </div>
        </div>

        <BForm @submit.stop.prevent="handleSubmit">
          <BFormGroup :label="t('changePassword.currentPassword')">
            <BFormInput
              type="password"
              v-model="currentPassword"
              :placeholder="t('changePassword.currentPasswordPlaceholder')"
              :state="v$.currentPassword.$error ? false : null"
              autocomplete="current-password"
              size="lg"
            />
            <BFormInvalidFeedback v-if="v$.currentPassword.required.$invalid">
              {{ t('changePassword.currentPasswordRequired') }}
            </BFormInvalidFeedback>
          </BFormGroup>

          <BFormGroup :label="t('changePassword.newPassword')">
            <BFormInput
              type="password"
              v-model="newPassword"
              :placeholder="t('changePassword.newPasswordPlaceholder')"
              :state="v$.newPassword.$error ? false : null"
              autocomplete="new-password"
              size="lg"
            />
            <BFormInvalidFeedback v-if="v$.newPassword.minLength.$invalid">
              {{ t('changePassword.passwordTooShort') }}
            </BFormInvalidFeedback>
            <BFormInvalidFeedback v-else-if="v$.newPassword.password_validator.$invalid">
              {{ t('changePassword.passwordRequirements') }}
            </BFormInvalidFeedback>
          </BFormGroup>

          <BFormGroup :label="t('changePassword.confirmPassword')">
            <BFormInput
              type="password"
              v-model="confirmPassword"
              :placeholder="t('changePassword.confirmPasswordPlaceholder')"
              :state="v$.confirmPassword.$error ? false : null"
              autocomplete="new-password"
              size="lg"
              @keyup.enter="handleSubmit"
            />
            <BFormInvalidFeedback v-if="v$.confirmPassword.sameAs.$invalid">
              {{ t('changePassword.passwordsDoNotMatch') }}
            </BFormInvalidFeedback>
          </BFormGroup>

          <BAlert
            variant="danger"
            :model-value="!!error"
            dismissible
            fade
            @update:model-value="error = null"
            class="mt-3"
          >
            <span class="alert-icon"><AppIcon name="close" /></span>
            {{ error }}
          </BAlert>

          <BButton
            variant="primary"
            block
            size="lg"
            type="submit"
            :disabled="loading"
          >
            <span v-if="loading" class="spinner-border spinner-border-sm me-2"></span>
            <span>{{ loading ? t('common.changing') : t('changePassword.changePassword') }}</span>
          </BButton>
        </BForm>
      </div>
    </section>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useVuelidate } from '@vuelidate/core'
import { required, minLength, sameAs, helpers } from '@vuelidate/validators'
import axios from 'axios'
import { useLoginStore } from './stores.js'

const { t } = useI18n()

const router = useRouter()
const loginStore = useLoginStore()

const currentPassword = ref('')
const newPassword = ref('')
const confirmPassword = ref('')
const error = ref(null)
const loading = ref(false)

const password_validator = helpers.regex(/^(?=.*[a-z])(?=.*[A-Z])(?=.*\d).{8,}$/)

const rules = computed(() => ({
  currentPassword: { required },
  newPassword: { required, minLength: minLength(8), password_validator },
  confirmPassword: { required, sameAs: sameAs(newPassword.value) }
}))

const v$ = useVuelidate(rules, { currentPassword, newPassword, confirmPassword })

const handleSubmit = async () => {
  v$.value.$touch()
  if (v$.value.$invalid || loading.value) return

  error.value = null
  loading.value = true

  try {
      const response = await axios.post('/api/change-password', {
        currentPassword: currentPassword.value,
        newPassword: newPassword.value
      })

    if (response.data.success) {
      // Update token in store
      if (response.data.token) {
        loginStore.login(response.data.token)
        loginStore.setPasswordChanged(true)
      }
      // Redirect to home
      router.push('/')
    } else {
      error.value = response.data.error || t('common.unknownError')
    }
  } catch (e) {
    // The firmware sends validation errors as a plain-text body, not JSON
    const data = e.response?.data
    error.value = (typeof data === 'string' && data) || data?.message || e.message || t('changePassword.changeError')
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.change-password-page {
  display: flex;
  flex-direction: column;
  gap: var(--spacing-lg);
  align-items: stretch;
}

.change-password-card {
  width: 100%;
  max-width: 560px;
  margin: 0 auto;
}

.card-body-content {
  padding: var(--card-padding);
  display: flex;
  flex-direction: column;
  gap: var(--spacing-md);
}

.warning-alert {
  padding: var(--spacing-md);
  margin: 0;
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
}

.alert-icon {
  font-size: var(--fs-xl);
  flex-shrink: 0;
  display: inline-flex;
}

.alert-content {
  flex: 1;
}

.alert-content strong {
  display: block;
  margin-bottom: var(--spacing-xs);
}

.password-requirements {
  display: flex;
  align-items: flex-start;
  gap: var(--spacing-md);
  padding: var(--spacing-md);
  background: var(--color-bg);
  border-radius: var(--radius-md);
}

.req-icon {
  font-size: var(--fs-xl);
  color: var(--color-text-secondary);
  display: inline-flex;
  flex-shrink: 0;
}

.req-content {
  flex: 1;
}

.req-content strong {
  display: block;
  margin-bottom: var(--spacing-xs);
  font-size: var(--fs-xs);
}

.req-content ul {
  margin: 0;
  padding-left: var(--spacing-lg);
  font-size: var(--fs-xs);
  color: var(--color-text-secondary);
}

.req-content li {
  margin-bottom: var(--spacing-xs);
}

@media (max-width: 480px) {
  .change-password-card {
    max-width: 100%;
  }
}
</style>
