idf_component_register(
    SRCS           "sd_driver.c"
    INCLUDE_DIRS   "."
    REQUIRES       platform
    PRIV_REQUIRES  fatfs sdmmc esp_driver_sdmmc
)
