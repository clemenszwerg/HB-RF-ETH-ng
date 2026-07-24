import de from './de.js'
import en from './en.js'
import fr from './fr.js'
import it from './it.js'

export const messages = {
  de,
  en,
  fr,
  it
}

export const availableLocales = [
  { code: 'de', name: 'Deutsch', flag: '\uD83C\uDDE9\uD83C\uDDEA' },
  { code: 'en', name: 'English', flag: '\uD83C\uDDEC\uD83C\uDDE7' },
  { code: 'fr', name: 'Fran\u00E7ais', flag: '\uD83C\uDDEB\uD83C\uDDF7' },
  { code: 'it', name: 'Italiano', flag: '\uD83C\uDDEE\uD83C\uDDF9' }
]

// Function to get browser language and map it to available locale
export function getBrowserLocale() {
  const browserLang = navigator.language || navigator.userLanguage
  const langCode = browserLang.split('-')[0]

  // Check if we have this language
  if (messages[langCode]) {
    return langCode
  }

  // Default to German
  return 'de'
}
