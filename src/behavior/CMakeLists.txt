idf_component_register(
    SRCS         "state_vector.c"
                 "idle_behavior.c"
    INCLUDE_DIRS "."
    REQUIRES     models face gaze_service
    PRIV_REQUIRES nvs_flash esp_timer freertos
)
