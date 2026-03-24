idf_component_register(
    SRCS          "inmp441_driver.c"
                  "max98357a_driver.c"
    INCLUDE_DIRS  "."
    REQUIRES      platform
    PRIV_REQUIRES driver
)
