idf_component_register(
    SRCS          "main.c"
    REQUIRES      config_manager
    PRIV_REQUIRES nvs_flash
    INCLUDE_DIRS  "."
)
