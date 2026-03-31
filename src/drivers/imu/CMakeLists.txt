idf_component_register(
    SRCS           "imu_driver.c"
    INCLUDE_DIRS   "."
    REQUIRES       platform i2c_bus
    PRIV_REQUIRES  esp_driver_i2c
)
