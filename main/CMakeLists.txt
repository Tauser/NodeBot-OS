idf_component_register(
    SRCS          "main.c"
                  "../src/drivers/imu/imu_driver.c"
    REQUIRES      platform
    PRIV_REQUIRES esp_driver_i2c
    INCLUDE_DIRS  "../src/drivers/imu"
)
