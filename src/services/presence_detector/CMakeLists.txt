idf_component_register(
    SRCS         "presence_detector.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus camera_service
    PRIV_REQUIRES esp_heap_caps freertos
)
