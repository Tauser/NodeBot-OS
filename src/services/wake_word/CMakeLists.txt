idf_component_register(
    SRCS           "wake_word.c"
    INCLUDE_DIRS   "."
    REQUIRES       event_bus face
    PRIV_REQUIRES  audio_capture espressif__esp-sr esp_timer
)
