idf_component_register(
    SRCS          "audio_capture.c"
                  "vad.c"
                  "audio_feedback.c"
                  "audio_playback_task.c"
                  "wake_word.c"
    INCLUDE_DIRS  "."
    REQUIRES      audio event_bus sd
    PRIV_REQUIRES freertos esp_timer nvs_flash espressif__esp-sr spiffs
)
