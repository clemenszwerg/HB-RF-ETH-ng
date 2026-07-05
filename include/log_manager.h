/*
 *  log_manager.h is part of the HB-RF-ETH firmware v2.0
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

#include <cstdint>
#include <string>
#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class LogManager {
public:
    static LogManager& instance();

    // Static wrappers for initialization (called from main.cpp / WebUI)
    static void begin(size_t size = 8192);
    // Frees the ring buffer and restores the default log sink. The buffer is
    // NOT allocated at boot (saves ~8 KB heap needed for the TLS handshake
    // during firmware update checks); the WebUI enables it on demand.
    static void stop();
    static void clear();

    // True while the in-memory ring buffer is active (i.e. begin() was called
    // and stop() has not freed it since). Read by the WebUI to show whether
    // log capture is running.
    bool isEnabled() const;

    // Instance methods
    std::string getLogContent(size_t offset = 0);
    size_t getTotalWritten() const;

private:
    LogManager();

    // Internal implementation
    void _begin(size_t size);
    void _stop();
    void _clear();
    void write(const char* data, size_t len);

    char *log_buffer = nullptr;
    size_t log_buffer_size = 0;
    uint64_t total_written = 0;
    mutable SemaphoreHandle_t _mutex = nullptr;

    // Original esp_log vprintf handler saved by begin() and restored by
    // stop(); typed as a plain function pointer to avoid pulling esp_log.h
    // into this header.
    int (*_orig_vprintf)(const char *, va_list) = nullptr;

    // Allow the C-style callback to access private members
    friend int log_vprintf(const char *fmt, va_list args);
};
