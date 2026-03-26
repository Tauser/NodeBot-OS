idf_component_register(
    SRCS         "motion_safety_service.c"
    INCLUDE_DIRS "."
    REQUIRES     servo event_bus
    PRIV_REQUIRES esp_timer freertos
)
