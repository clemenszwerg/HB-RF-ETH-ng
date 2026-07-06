/*
 *  supporter_key.cpp is part of the HB-RF-ETH firmware v2.0
 *
 *  Original work Copyright 2022 Alexander Reinert
 *  https://github.com/alexreinert/HB-RF-ETH
 *
 *  Modified work Copyright 2025 Xerolux
 *  Modernized fork - Updated to ESP-IDF 6.0 and modern toolchains
 *
 *  The HB-RF-ETH firmware is licensed under a
 *  Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 *
 *  You should have received a copy of the license along with this
 *  work.  If not, see <http://creativecommons.org/licenses/by-nc-sa/4.0/>.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include "supporter_key.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// Base32 alphabet without ambiguous characters (no 0/O, no 1/I). The key is
// shown to users as XXXX-XXXX-XXXX-XXXX, 16 chars = 80 bits.
static const char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
static const int ALPHABET_LEN = 32;

// The supporter key epoch. Day offsets in the key are counted from this date
// (UTC). Picked well in the past so generated keys never produce a negative
// offset, and far enough forward (uint16_t = 65535 days ≈ 179 years) that the
// scheme never runs out of range.
#define SK_EPOCH_YEAR  2025
#define SK_EPOCH_MONTH 1
#define SK_EPOCH_DAY   1

static int8_t char_to_val(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (int i = 0; i < ALPHABET_LEN; i++)
    {
        if (ALPHABET[i] == c)
            return (int8_t)i;
    }
    return -1;
}

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no final XOR, MSB-first).
// Must match the Python generator exactly.
static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x8000)
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            else
                crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

// Howard Hinnant's days_from_civil: {y, m, d} -> days since 1970-01-01.
// Pure arithmetic, timezone-independent.
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

// civil_from_days: days since 1970-01-01 -> {y, m, d}. Inverse of the above.
static void civil_from_days(int64_t z, int *y, int *m, int *d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t yy = (int64_t)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned dd = doy - (153 * mp + 2) / 5 + 1;
    const unsigned mm = mp < 10 ? mp + 3 : mp - 9;
    if (mm <= 2)
        yy += 1;
    *y = (int)yy;
    *m = (int)mm;
    *d = (int)dd;
}

void supporter_key_format_expiry(uint16_t dayOffset, char *out, size_t outSize)
{
    if (!out || outSize < 11)
        return;
    const int64_t epoch_days = days_from_civil(SK_EPOCH_YEAR, SK_EPOCH_MONTH, SK_EPOCH_DAY);
    int y, m, d;
    civil_from_days(epoch_days + dayOffset, &y, &m, &d);
    snprintf(out, outSize, "%04d-%02d-%02d", y, m, d);
}

bool supporter_key_validate(const char *key, SupporterKeyStatus &status)
{
    memset(&status, 0, sizeof(status));
    status.expiresAt[0] = '\0';

    if (!key)
        return false;

    // Collect exactly 16 base32 characters, ignoring dashes / spaces. Any
    // extra valid character beyond 16 (or fewer than 16) rejects the key.
    uint8_t vals[16];
    int count = 0;
    for (size_t i = 0; key[i] != '\0'; i++)
    {
        char c = key[i];
        if (c == '-' || c == ' ')
            continue;
        int8_t v = char_to_val(c);
        if (v < 0)
            return false;
        if (count >= 16)
            return false; // too long
        vals[count++] = (uint8_t)v;
    }

    if (count != 16)
        return false;

    // Pack 16 x 5-bit values into 10 bytes (MSB-first).
    uint8_t bytes[10] = {0};
    int bit_pos = 0;
    for (int i = 0; i < 16; i++)
    {
        for (int b = 4; b >= 0; b--)
        {
            int bit = (vals[i] >> b) & 1;
            int byte_idx = bit_pos / 8;
            int bit_idx = 7 - (bit_pos % 8);
            if (bit)
                bytes[byte_idx] |= (uint8_t)(1 << bit_idx);
            bit_pos++;
        }
    }

    // Verify CRC16 over bytes[2..9] matches bytes[0..1].
    const uint16_t stored_crc = ((uint16_t)bytes[0] << 8) | bytes[1];
    const uint16_t calc_crc = crc16_ccitt(&bytes[2], 8);
    if (stored_crc != calc_crc)
        return false;

    status.valid = true;
    status.expiryDay = (uint16_t)(((uint16_t)bytes[2] << 8) | bytes[3]);
    supporter_key_format_expiry(status.expiryDay, status.expiresAt, sizeof(status.expiresAt));

    // Evaluate expiry against the system clock (UTC day boundaries).
    const int64_t epoch_days = days_from_civil(SK_EPOCH_YEAR, SK_EPOCH_MONTH, SK_EPOCH_DAY);
    time_t now = time(NULL);
    int64_t today_offset;
    if (now <= 0)
    {
        // Clock not initialised at all (fresh boot, no NTP yet).
        status.clockUnknown = true;
        status.active = true;
        status.expired = false;
        return true;
    }
    const int64_t today_days = (int64_t)(now / 86400);
    today_offset = today_days - epoch_days;
    if (today_offset < 0)
    {
        // System clock is set but before our epoch (e.g. 1970 without NTP).
        // Treat as "clock unknown" so a freshly flashed device still shows the
        // badge until NTP catches up.
        status.clockUnknown = true;
        status.active = true;
        status.expired = false;
    }
    else
    {
        status.expired = (today_offset > status.expiryDay);
        status.active = !status.expired;
    }
    return true;
}
