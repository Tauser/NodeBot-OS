idf_component_register(
    SRCS         "cloud_bridge.c"
    INCLUDE_DIRS "."
    REQUIRES     wifi_manager
    PRIV_REQUIRES esp_http_client mbedtls nvs_flash esp_timer freertos esp_rom
)
