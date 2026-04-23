# CMakeLists.txt for HB-RF-ETH-ng firmware
# Updated for ESP-IDF 6.0 - explicit component dependencies required

FILE(GLOB_RECURSE app_sources ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS "../include"
                       REQUIRES
                           driver
                           esp_eth
                           esp_netif
                           esp_event
                           esp_http_server
                           esp_http_client
                           esp_https_ota
                           esp_timer
                           nvs_flash
                           mqtt
                           lwip
                           mdns
                           mbedtls
                           json
                           app_update
                           esp_adc
                           )

target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.css.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/main.js.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/index.html.gz" BINARY)
target_add_binary_data(${COMPONENT_TARGET} "../webui/dist/favicon.ico.gz" BINARY)
