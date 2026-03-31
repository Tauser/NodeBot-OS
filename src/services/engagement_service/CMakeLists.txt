idf_component_register(
    SRCS         "engagement_service.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus
    PRIV_REQUIRES freertos esp_timer
)
