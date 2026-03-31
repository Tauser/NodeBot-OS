idf_component_register(
    SRCS         "preference_memory_service.c"
    INCLUDE_DIRS "."
    REQUIRES     nvs_flash sd
    PRIV_REQUIRES esp_timer
)
