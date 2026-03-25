idf_component_register(
    SRCS           "face_engine.cpp"
                   "face_renderer.cpp"
                   "face_debug.cpp"
                   "blink_controller.cpp"
    INCLUDE_DIRS   "."
    REQUIRES       display LovyanGFX freertos models
    PRIV_REQUIRES  esp_timer watchdog_manager
)
