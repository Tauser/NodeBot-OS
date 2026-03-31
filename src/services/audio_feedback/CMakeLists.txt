idf_component_register(
    SRCS           "audio_feedback.c"
    INCLUDE_DIRS   "."
    REQUIRES       audio sd audio_capture
    PRIV_REQUIRES  freertos
)
