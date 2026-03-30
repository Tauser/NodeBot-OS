idf_component_register(
    SRCS         "state_vector.c"
                 "idle_behavior.c"
                 "gesture_service.c"
    INCLUDE_DIRS "."
    REQUIRES     models face gaze_service event_bus
    PRIV_REQUIRES nvs_flash esp_timer freertos
)
