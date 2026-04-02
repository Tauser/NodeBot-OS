idf_component_register(
    SRCS         "intent_mapper.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus audio_capture cloud_bridge
)
