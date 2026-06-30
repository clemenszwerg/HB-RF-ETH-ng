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
