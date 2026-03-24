idf_component_register(
    SRCS           "display.cpp"
                   "face_engine.cpp"
    INCLUDE_DIRS   "."
    REQUIRES       platform LovyanGFX
    PRIV_REQUIRES  esp_timer
)
