/*
 *  secure_utils.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef SECURE_UTILS_H
#define SECURE_UTILS_H

#include <stddef.h>
#include <string.h>

/**
 * @brief Securely zeros out memory to prevent compiler optimization
 *
 * @param v Pointer to memory to zero
 * @param n Number of bytes to zero
 */
static inline void secure_zero(void *v, size_t n) {
    volatile unsigned char *p = (volatile unsigned char *)v;
    while (n--) *p++ = 0;
}

/**
 * @brief Constant-time string comparison to prevent timing attacks.
 *
 * The comparison never short-circuits on length and does not call strlen,
 * so an attacker cannot learn the length of the inputs from timing.
 *
 * @param a First string (NULL-safe)
 * @param b Second string (NULL-safe)
 * @return 0 if strings are equal, non-zero otherwise
 */
static inline int secure_strcmp(const char *a, const char *b) {
    if (!a || !b) return 1;

    // Walk both strings in lock-step without calling strlen (which leaks
    // length via timing) and without short-circuiting on the first mismatch.
    // Accumulate differences and track whether each string has ended. The loop
    // stops once both strings have terminated, so the runtime only reveals the
    // length of the longer string, not the position of any difference.
    int result = 0;
    int a_ended = 0;
    int b_ended = 0;
    size_t i = 0;

    do {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        a_ended |= (ca == 0);
        b_ended |= (cb == 0);
        result |= (ca ^ cb);
        i++;
    } while (!(a_ended && b_ended));

    // Strings are equal only if every byte matched AND they ended at the same position.
    return result | (a_ended ^ b_ended);
}

#endif // SECURE_UTILS_H
