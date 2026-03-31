idf_component_register(
    SRCS         "state_vector.c"
                 "idle_behavior.c"
                 "gesture_service.c"
                 "behavior_tree.c"
                 "behavior_engine.c"
    INCLUDE_DIRS "."
    REQUIRES     models face gaze_service event_bus motion_safety dialogue_state safe_mode
    PRIV_REQUIRES nvs_flash esp_timer freertos
)
