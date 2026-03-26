idf_component_register(
    SRCS         "touch_service.c"
    INCLUDE_DIRS "."
    REQUIRES     touch event_bus face behavior
    PRIV_REQUIRES models freertos esp_timer nvs_flash
)
