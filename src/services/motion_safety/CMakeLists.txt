idf_component_register(
    SRCS         "motion_safety_service.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus
    PRIV_REQUIRES freertos esp_timer
)
