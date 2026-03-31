idf_component_register(
    SRCS         "touch_service.c"
    INCLUDE_DIRS "."
    REQUIRES     touch event_bus face behavior config_manager
    PRIV_REQUIRES freertos esp_timer
)
