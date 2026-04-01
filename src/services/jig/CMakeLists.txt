idf_component_register(
    SRCS         "jig_service.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES face audio_capture audio_feedback sd wifi_manager led imu_service motion_safety esp_wifi freertos
)

# Jig de fábrica habilitado com cmake -DJIG_BUILD=1
if(JIG_BUILD)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC JIG_BUILD)
    message(STATUS "jig: JIG_BUILD habilitado — jig_service ativo")
endif()
