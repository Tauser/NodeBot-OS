idf_component_register(
    SRCS         "brownout_handler.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash esp_system
)
