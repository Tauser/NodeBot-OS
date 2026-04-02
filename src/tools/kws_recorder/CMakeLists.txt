idf_component_register(
    SRCS         "kws_recorder.c"
    INCLUDE_DIRS "."
    REQUIRES     audio_capture intent_mapper
)
