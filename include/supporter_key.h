/*
 *  supporter_key.h is part of the HB-RF-ETH firmware v2.0
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

#pragma once

#include <stdint.h>
#include <stddef.h>

// Result of validating a supporter key. The key encodes an expiry date and a
// CRC16 checksum so the firmware can verify it fully offline — no network,
// no server, no shared secret. This is intentionally NOT cryptographically
// secure: it only protects against typos and casual guessing. The supporter
// badge is purely cosmetic (no functionality is ever locked), so a weak
// scheme is acceptable and keeps the implementation tiny.
struct SupporterKeyStatus
{
    // True when the badge should currently light up: checksum is valid AND
    // the key has not expired (or the system clock is not yet synced, in
    // which case we cannot check the expiry and trust the checksum only).
    bool active;
    // True if the checksum is valid (key is well-formed), regardless of expiry.
    bool valid;
    // True if the checksum is valid but the key is past its expiry date.
    bool expired;
    // True if the system clock is not yet set (before 2025-01-01 UTC), so the
    // expiry could not be evaluated. active falls back to "valid" in that case.
    bool clockUnknown;
    // Raw expiry encoded in the key: days since 2025-01-01 (UTC).
    uint16_t expiryDay;
    // Human-readable expiry date "YYYY-MM-DD" (UTC), or "" if invalid.
    char expiresAt[11];
};

// Parses and validates a supporter key string. Accepts messy user input:
// dashes and spaces are stripped, lowercase is upper-cased. Fills `status`.
// Returns true when the checksum is valid (status.valid). Returns false for
// malformed input, wrong length, or checksum mismatch.
bool supporter_key_validate(const char *key, SupporterKeyStatus &status);

// Formats a human-readable expiry date "YYYY-MM-DD" (UTC) from a day offset
// counted from 2025-01-01. Pure date math, no time.h / timezone dependency.
void supporter_key_format_expiry(uint16_t dayOffset, char *out, size_t outSize);
