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
        imu
        display
        face
        touch
        led
        audio
        gaze_service
        behavior
        safe_mode
        motion_safety
        imu_service
        touch_service
)
