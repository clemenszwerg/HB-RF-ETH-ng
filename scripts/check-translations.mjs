import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..')
const localeDir = path.join(root, 'webui', 'src', 'locales')
const sourceDir = path.join(root, 'webui', 'src')
let failed = false
const fail = (message) => { console.error(`Translation check failed: ${message}`); failed = true }

const loadLocale = (file) => {
  const source = fs.readFileSync(file, 'utf8')
  const transformed = source.replace(/^\s*export\s+default\s+/, 'return ')
  if (transformed === source) throw new Error(`Unsupported locale module: ${file}`)
  return Function(`"use strict";\n${transformed}`)()
}

const flatten = (value, prefix = '', keys = new Set()) => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    if (prefix) keys.add(prefix)
    return keys
  }
  for (const [key, nested] of Object.entries(value)) {
    const full = prefix ? `${prefix}.${key}` : key
    if (nested && typeof nested === 'object' && !Array.isArray(nested)) flatten(nested, full, keys)
    else keys.add(full)
  }
  return keys
}

const walk = (directory) => fs.readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
  const file = path.join(directory, entry.name)
  return entry.isDirectory() ? walk(file) : /\.(?:js|vue)$/.test(entry.name) ? [file] : []
})

const localeFiles = fs.readdirSync(localeDir).filter((name) => /^[a-z]{2}\.js$/.test(name)).sort()
const localeKeys = Object.fromEntries(localeFiles.map((name) => [name.slice(0, 2), flatten(loadLocale(path.join(localeDir, name)))]))
const english = localeKeys.en
if (!english) fail('webui/src/locales/en.js is missing')

const used = new Set()
const callPattern = /(?:^|[^\w$])(?:t|\$t|translate|i18n\.global\.t)\(\s*(['"`])([^'"`]+)\1/gm
for (const file of walk(sourceDir)) {
  const source = fs.readFileSync(file, 'utf8')
  for (const match of source.matchAll(callPattern)) {
    if (!match[2].includes('${')) used.add(match[2])
  }
}

if (english) {
  const missingEnglish = [...used].filter((key) => !english.has(key)).sort()
  if (missingEnglish.length) fail(`English keys missing:\n  ${missingEnglish.join('\n  ')}`)
  for (const [code, keys] of Object.entries(localeKeys)) {
    if (code === 'en') continue
    const absentInEnglish = [...keys].filter((key) => !english.has(key)).sort()
    if (absentInEnglish.length) fail(`${code}.js has keys missing in en.js:\n  ${absentInEnglish.join('\n  ')}`)
    const missing = [...english].filter((key) => !keys.has(key))
    if (missing.length) console.warn(`${code}.js misses ${missing.length} English reference keys; runtime fallback is English.`)
  }
}

const main = fs.readFileSync(path.join(sourceDir, 'main.js'), 'utf8')
const index = fs.readFileSync(path.join(localeDir, 'index.js'), 'utf8')
if (!/fallbackLocale\s*:\s*['"]en['"]/.test(main)) fail('Vue-I18n fallbackLocale must be en')
if (!/Default to German[\s\S]*return\s+['"]de['"]/.test(index)) fail('Unknown browser locales must default to German')

if (failed) process.exit(1)
console.log(`Translation check passed: ${used.size} used keys, ${english.size} English keys, ${localeFiles.length} locales.`)
