idf_component_register(
    SRCS          "watchdog_manager.c"
    INCLUDE_DIRS  "."
    PRIV_REQUIRES esp_task_wdt
)
