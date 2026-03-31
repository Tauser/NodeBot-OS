idf_component_register(
    SRCS         "led_router.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus led
    PRIV_REQUIRES esp_driver_gpio
)
