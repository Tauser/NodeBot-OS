idf_component_register(
    SRCS          "audio_driver.c"
    INCLUDE_DIRS  "."
    REQUIRES      platform
    PRIV_REQUIRES driver
)
