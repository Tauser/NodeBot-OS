idf_component_register(
    SRCS         "face_tracker.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus presence_detector behavior motion_safety dialogue_state
    PRIV_REQUIRES freertos esp_timer
)
