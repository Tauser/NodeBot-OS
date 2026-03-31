idf_component_register(
    SRCS         "i2c_bus.c"
    INCLUDE_DIRS "."
    REQUIRES     platform
    PRIV_REQUIRES driver
)
