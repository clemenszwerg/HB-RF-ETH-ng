/*
 *  semver.h is part of the HB-RF-ETH firmware v2.0
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

#ifndef SEMVER_H
#define SEMVER_H

#include <stdio.h>
#include <string.h>

// Compare two pre-release identifiers per semver spec §11:
// - Numeric identifiers compare as integers
// - Non-numeric identifiers compare lexicographically in ASCII order
// - Numeric identifiers have lower precedence than non-numeric
// - A longer set of fields has higher precedence if all preceding fields are equal
static inline int _comparePreRelease(const char* p1, const char* p2) {
    if (!p1 && !p2) return 0;
    if (!p1) return 1;   // no pre-release = release = higher precedence
    if (!p2) return -1;

    // Tokenise by '.' (semver pre-release fields are dot-separated).
    // We walk both strings in parallel, extracting one field at a time.
    // This avoids allocating copies.
    const char *s1 = p1, *s2 = p2;
    while (*s1 && *s2) {
        // Find end of current field
        const char *e1 = s1, *e2 = s2;
        while (*e1 && *e1 != '.') e1++;
        while (*e2 && *e2 != '.') e2++;

        size_t l1 = (size_t)(e1 - s1);
        size_t l2 = (size_t)(e2 - s2);

        // Check if both fields are purely numeric
        int n1_isNum = 1, n2_isNum = 1;
        for (size_t i = 0; i < l1; i++) if (s1[i] < '0' || s1[i] > '9') { n1_isNum = 0; break; }
        for (size_t i = 0; i < l2; i++) if (s2[i] < '0' || s2[i] > '9') { n2_isNum = 0; break; }

        int cmp;
        if (n1_isNum && n2_isNum) {
            // Numeric comparison
            int num1 = 0, num2 = 0;
            for (size_t i = 0; i < l1; i++) num1 = num1 * 10 + (s1[i] - '0');
            for (size_t i = 0; i < l2; i++) num2 = num2 * 10 + (s2[i] - '0');
            cmp = (num1 > num2) - (num1 < num2);
        } else if (n1_isNum != n2_isNum) {
            // Numeric identifiers have lower precedence
            return n1_isNum ? -1 : 1;
        } else {
            // Both non-numeric: lexicographic
            size_t min = l1 < l2 ? l1 : l2;
            cmp = strncmp(s1, s2, min);
            if (cmp == 0) cmp = (l1 > l2) - (l1 < l2);
        }

        if (cmp != 0) return cmp;

        // Advance past the dot
        s1 = *e1 ? e1 + 1 : e1;
        s2 = *e2 ? e2 + 1 : e2;
    }

    // Fewer fields means lower precedence (e.g. "Beta" < "Beta.1")
    return (*s1 ? 1 : 0) - (*s2 ? 1 : 0);
}

static inline int compareVersions(const char* v1, const char* v2) {
    int v1_parts[3] = {0, 0, 0};
    int v2_parts[3] = {0, 0, 0};
    const char* v1_pre = NULL;
    const char* v2_pre = NULL;

    // Parse major.minor.patch and optional pre-release tag
    (void)sscanf(v1, "%d.%d.%d", &v1_parts[0], &v1_parts[1], &v1_parts[2]);
    (void)sscanf(v2, "%d.%d.%d", &v2_parts[0], &v2_parts[1], &v2_parts[2]);

    // Find pre-release suffix (e.g. "-Beta.1")
    const char* dash = strchr(v1, '-');
    if (dash) v1_pre = dash + 1;
    dash = strchr(v2, '-');
    if (dash) v2_pre = dash + 1;

    for (int i = 0; i < 3; i++) {
        if (v1_parts[i] > v2_parts[i]) return 1;
        if (v1_parts[i] < v2_parts[i]) return -1;
    }

    // Pre-release versions are lower than release versions (semver spec)
    if (!v1_pre && !v2_pre) return 0;
    if (!v1_pre) return 1;
    if (!v2_pre) return -1;

    // Both have pre-release tags: compare per semver §11
    return _comparePreRelease(v1_pre, v2_pre);
}

#endif
