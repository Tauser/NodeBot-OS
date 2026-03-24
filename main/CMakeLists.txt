idf_component_register(
    SRCS          "main.c"
    REQUIRES      log_manager
    PRIV_REQUIRES sd esp_timer
    INCLUDE_DIRS  "."
)
