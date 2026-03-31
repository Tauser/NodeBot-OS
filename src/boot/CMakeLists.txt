idf_component_register(
    SRCS          "boot_sequence.c"
    INCLUDE_DIRS  "."
                  "../drivers/audio"
    PRIV_REQUIRES
        platform
        config_manager
        log_manager
        event_bus
        sd
        imu_service
        audio_capture
        audio_feedback
        wake_word
        display
        face
        touch_service
        led
        audio
        gaze_service
        behavior
        brownout_handler
        safe_mode
        led_router
)
