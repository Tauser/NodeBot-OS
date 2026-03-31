idf_component_register(
    SRCS         "camera_service.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus platform
    PRIV_REQUIRES esp32-camera freertos esp_timer esp_psram
)
