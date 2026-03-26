idf_component_register(
    SRCS          "audio_capture.c"
                  "vad.c"
    INCLUDE_DIRS  "."
    REQUIRES      audio event_bus
    PRIV_REQUIRES freertos esp_timer nvs_flash
)
