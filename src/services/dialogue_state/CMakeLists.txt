idf_component_register(
    SRCS           "dialogue_state_service.c"
    INCLUDE_DIRS   "."
    REQUIRES       event_bus tts
    PRIV_REQUIRES  intent_mapper esp_timer freertos
)
