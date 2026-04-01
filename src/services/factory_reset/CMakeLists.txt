idf_component_register(
    SRCS         "factory_reset.c"
                 "smoke_tests.c"
    INCLUDE_DIRS "."
    REQUIRES     event_bus touch_service
    PRIV_REQUIRES nvs_flash esp_timer freertos led audio_capture audio_feedback sd wifi_manager led_router motion_safety imu_service face models
)

# Smoke tests habilitados com cmake -DQA_BUILD=1
if(QA_BUILD)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC QA_BUILD)
    message(STATUS "factory_reset: QA_BUILD habilitado — smoke_tests ativos")
endif()
