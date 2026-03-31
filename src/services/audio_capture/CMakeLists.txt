idf_component_register(
    SRCS           "audio_capture.c"
                   "vad.c"
    INCLUDE_DIRS   "."
    REQUIRES       audio event_bus config_manager
    PRIV_REQUIRES  esp_timer
)
