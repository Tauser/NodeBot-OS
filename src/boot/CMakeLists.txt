idf_component_register(
    SRCS          "boot_sequence.c"
    INCLUDE_DIRS  "."
    PRIV_REQUIRES
        platform
        config_manager
        log_manager
        event_bus
        sd
        imu
        display
        touch
        led
        audio
)
