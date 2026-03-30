idf_component_register(
    SRCS         "safe_mode_service.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_timer
)
