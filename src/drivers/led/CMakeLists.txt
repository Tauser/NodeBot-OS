idf_component_register(
    SRCS           "ws2812_driver.c"
    INCLUDE_DIRS   "."
    REQUIRES       platform
    PRIV_REQUIRES  esp_driver_rmt esp_driver_gpio esp_timer freertos
)
