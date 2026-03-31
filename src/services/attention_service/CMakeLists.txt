idf_component_register(
    SRCS         "attention_service.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus gaze_service touch_service touch
)
