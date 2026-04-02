cmake_minimum_required(VERSION 3.22)

# Extra component search paths (one level deep each)
set(EXTRA_COMPONENT_DIRS
    src
    src/core
    src/drivers
    src/services
    src/diagnostics
    src/tools
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(NodeBot)

# Fix: GCC ICE em esp_lcd_panel_rgb.c com otimização agressiva.
# TARGET_DIRECTORY necessário para alcançar arquivos de componentes IDF (CMake 3.18+).
idf_component_get_property(_esp_lcd_lib esp_lcd COMPONENT_LIB)
if(_esp_lcd_lib)
    set_source_files_properties(
        "$ENV{IDF_PATH}/components/esp_lcd/rgb/esp_lcd_panel_rgb.c"
        TARGET_DIRECTORY ${_esp_lcd_lib}
        PROPERTIES COMPILE_FLAGS "-O1")
endif()

# Fix: lgfx_qrcode.h usa "typedef unsigned char bool", incompatível com C23.
# Patch idempotente aplicado em configure time.
set(_qrcode_h "${CMAKE_CURRENT_SOURCE_DIR}/components/LovyanGFX/src/lgfx/utility/lgfx_qrcode.h")
if(EXISTS "${_qrcode_h}")
    file(READ "${_qrcode_h}" _qr_content)
    if(NOT _qr_content MATCHES "202311L")
        string(REPLACE
            "#ifndef __cplusplus\ntypedef unsigned char bool;"
            "#ifndef __cplusplus\n#if __STDC_VERSION__ < 202311L\ntypedef unsigned char bool;"
            _qr_content "${_qr_content}")
        string(REPLACE
            "static const bool true = 1;\n#endif"
            "static const bool true = 1;\n#endif\n#endif"
            _qr_content "${_qr_content}")
        file(WRITE "${_qrcode_h}" "${_qr_content}")
        message(STATUS "Patched lgfx_qrcode.h for C23 compatibility")
    endif()
endif()

