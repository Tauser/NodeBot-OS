idf_component_register(
    SRCS         "ota_manager.c"
    INCLUDE_DIRS "."
    REQUIRES     motion_safety dialogue_state models
    PRIV_REQUIRES esp_http_client mbedtls esp_timer app_update freertos
)
