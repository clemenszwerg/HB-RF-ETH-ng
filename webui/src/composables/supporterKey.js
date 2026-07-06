// Client-side mirror of main/supporter_key.cpp. Used by the settings page to
// give instant feedback (valid / invalid / expired) without a round-trip, and
// to avoid sending an obviously bad key to the device. The backend re-checks
// the key before storing it, so this is purely for UX — never a source of
// truth.
//
// Keep in sync with tools/generate_supporter_key.py and main/supporter_key.cpp:
//   - alphabet: ABCDEFGHJKLMNPQRSTUVWXYZ23456789 (no 0/O/1/I)
//   - CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF)
//   - epoch: 2025-01-01 UTC, day offsets

const ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'
const EPOCH = Date.UTC(2025, 0, 1) / 86400000 // 2025-01-01 as days since 1970-01-01

function crc16ccitt(data) {
  let crc = 0xffff
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i] << 8
    for (let b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
    }
  }
  return crc & 0xffff
}

function civilFromDays(z) {
  z += 719468
  const era = Math.trunc((z >= 0 ? z : z - 146096) / 146097)
  const doe = z - era * 146097
  const yoe = Math.trunc((doe - Math.trunc(doe / 1460) + Math.trunc(doe / 36524) - Math.trunc(doe / 146096)) / 365)
  let y = yoe + era * 400
  const doy = doe - (365 * yoe + Math.trunc(yoe / 4) - Math.trunc(yoe / 100))
  const mp = Math.trunc((5 * doy + 2) / 153)
  const d = doy - Math.trunc((153 * mp + 2) / 5) + 1
  const m = mp < 10 ? mp + 3 : mp - 9
  if (m <= 2) y += 1
  return { y, m, d }
}

function formatExpiry(dayOffset) {
  const { y, m, d } = civilFromDays(EPOCH + dayOffset)
  const mm = String(m).padStart(2, '0')
  const dd = String(d).padStart(2, '0')
  return `${y}-${mm}-${dd}`
}

export function validateSupporterKey(rawKey) {
  const result = { valid: false, active: false, expired: false, clockUnknown: false, expiresAt: '' }
  if (!rawKey || typeof rawKey !== 'string') return result

  // Collect up to 16 base32 chars, ignoring dashes/spaces, case-insensitive.
  const vals = []
  for (const ch of rawKey) {
    if (ch === '-' || ch === ' ') continue
    const up = ch.toUpperCase()
    const idx = ALPHABET.indexOf(up)
    if (idx < 0) return result
    if (vals.length >= 16) return result // too long
    vals.push(idx)
  }
  if (vals.length !== 16) return result

  // Pack 16 x 5-bit values into 10 bytes (MSB-first).
  const bytes = new Uint8Array(10)
  let bitPos = 0
  for (let i = 0; i < 16; i++) {
    for (let b = 4; b >= 0; b--) {
      const bit = (vals[i] >> b) & 1
      if (bit) bytes[Math.trunc(bitPos / 8)] |= (1 << (7 - (bitPos % 8)))
      bitPos++
    }
  }

  const storedCrc = (bytes[0] << 8) | bytes[1]
  const calcCrc = crc16ccitt(bytes.subarray(2))
  if (storedCrc !== calcCrc) return result

  result.valid = true
  const expiryDay = (bytes[2] << 8) | bytes[3]
  result.expiresAt = formatExpiry(expiryDay)

  const nowDays = Math.trunc(Date.now() / 86400000)
  const todayOffset = nowDays - EPOCH
  if (todayOffset < 0) {
    // Clock before epoch — trust the checksum, can't check expiry.
    result.clockUnknown = true
    result.active = true
  } else {
    result.expired = todayOffset > expiryDay
    result.active = !result.expired
  }
  return result
}

// Pretty-print a key with dashes as the user types, ignoring existing dashes.
export function normalizeSupporterKey(value) {
  if (!value) return ''
  const clean = String(value).replace(/[\s-]/g, '').toUpperCase().split('').filter(c => ALPHABET.includes(c))
  const capped = clean.slice(0, 16)
  return capped.match(/.{1,4}/g)?.join('-') ?? ''
}
