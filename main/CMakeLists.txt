idf_component_register(
    SRCS          "main.c"
    REQUIRES      boot
    PRIV_REQUIRES display face
    INCLUDE_DIRS  "."
)
