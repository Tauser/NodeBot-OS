idf_component_register(
    SRCS           "tts.c"
    INCLUDE_DIRS   "."
    REQUIRES       event_bus
    PRIV_REQUIRES  audio audio_feedback wake_word sd audio_capture freertos
)
