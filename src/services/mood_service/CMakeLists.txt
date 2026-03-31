idf_component_register(
    SRCS         "mood_service.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus
    PRIV_REQUIRES freertos
)
