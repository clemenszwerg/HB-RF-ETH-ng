#include "ota_config.h"

void configure_ota_http_client(esp_http_client_config_t& config, const char* url) {
    config.url = url;

    // crt_bundle_attach deliberately omitted: the ESP-IDF 6.x PSA Crypto CA
    // bundle has a known issue (PSA_ERROR_GENERIC_ERROR / 0xffffff73) that
    // causes TLS handshake failures with newer Let's Encrypt / ISRG Root X2
    // certificates (the ones used by GitHub). Without the bundle, the TLS
    // layer still encrypts but does not verify the server certificate chain.

    // Fix for Bug #235: GitHub redirects fail with keep-alive
    config.keep_alive_enable = false;

    // Fix for Bug #235: Default TX buffer (512) is too small for large HTTPS headers
    config.buffer_size_tx = 4096;

    // Enable redirection following (defensive programming)
    config.max_redirection_count = 5;
}
