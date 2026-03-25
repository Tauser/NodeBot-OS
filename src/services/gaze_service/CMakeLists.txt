idf_component_register(
    SRCS         "gaze_service.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus face
    PRIV_REQUIRES freertos esp_timer
)
