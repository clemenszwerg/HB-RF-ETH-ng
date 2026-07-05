/*
 *  security_headers.h is part of the HB-RF-ETH firmware v2.0
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

#include <esp_http_server.h>

/**
 * Adds standard security headers to the HTTP response.
 *
 * Headers added:
 * - Content-Security-Policy: Strict policy to prevent XSS (default-src 'self'...)
 * - X-Content-Type-Options: nosniff (Prevents MIME sniffing)
 * - X-Frame-Options: SAMEORIGIN (Prevents Clickjacking)
 * - X-XSS-Protection: 1; mode=block (Legacy XSS protection)
 * - Referrer-Policy: strict-origin-when-cross-origin (Privacy protection)
 */
static inline void add_security_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self'; img-src 'self' data:; connect-src 'self' ws: wss:; font-src 'self' data:; object-src 'none'; base-uri 'self';");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "X-Frame-Options", "SAMEORIGIN");
    httpd_resp_set_hdr(req, "X-XSS-Protection", "1; mode=block");
    httpd_resp_set_hdr(req, "Referrer-Policy", "strict-origin-when-cross-origin");
}
