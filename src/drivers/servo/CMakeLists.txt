idf_component_register(
    SRCS         "scs0009_driver.c"
    INCLUDE_DIRS "."
    REQUIRES     platform
    PRIV_REQUIRES driver freertos
)
