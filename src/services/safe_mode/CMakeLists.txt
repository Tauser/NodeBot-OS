idf_component_register(
    SRCS         "brownout_handler.c"
                 "safe_mode_service.c"
    INCLUDE_DIRS "."
    REQUIRES     config_manager face
    PRIV_REQUIRES log_manager esp_timer esp_system
)
