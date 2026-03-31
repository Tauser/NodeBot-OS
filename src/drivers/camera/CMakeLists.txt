idf_component_register(
    SRCS         "camera_bringup.c"
    INCLUDE_DIRS "."
    REQUIRES     platform
    PRIV_REQUIRES esp32-camera freertos esp_timer
)
