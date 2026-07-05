/*
 *  rate_limiter.h is part of the HB-RF-ETH firmware v2.0
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
#include <stdbool.h>

// Rate limiter configuration
#define MAX_LOGIN_ATTEMPTS 5
#define RATE_LIMIT_WINDOW_SECONDS 60
#define MAX_TRACKED_IPS 20

#include "lwip/ip_addr.h"

// Rate limiter functions
void rate_limiter_init();
bool rate_limiter_check_login(httpd_req_t *req);
bool rate_limiter_is_whitelisted(httpd_req_t *req, const ip4_addr_t *whitelist_ip);
void rate_limiter_reset_ip(httpd_req_t *req);
void rate_limiter_cleanup();
