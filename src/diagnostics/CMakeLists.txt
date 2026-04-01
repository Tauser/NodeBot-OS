idf_component_register(
    SRCS         "snapshot_service.c"
                 "diagnostic_mode.c"
                 "telemetry_service.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus behavior
    PRIV_REQUIRES sd nvs_flash esp_timer freertos
)
