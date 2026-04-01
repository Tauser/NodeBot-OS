#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * JigService — E41
 *
 * Interface serial para jig de fábrica. Ativado com -DJIG_BUILD.
 * Fora do JIG_BUILD, jig_service_init() é no-op (retorna ESP_OK).
 *
 * Protocolo (linha terminada em \n):
 *   Host  → FW : TEST_<PERIFERICO>
 *   FW    → Host: <PERIFERICO>_OK [dados]
 *                 <PERIFERICO>_FAIL: motivo
 *
 * Ao inicializar imprime "BOOT_OK" no serial.
 *
 * Comandos suportados:
 *   TEST_DISPLAY  TEST_SERVO   TEST_MIC    TEST_SPEAKER  TEST_SD
 *   TEST_BATTERY  TEST_IMU     TEST_LED    TEST_WIFI
 *
 * jig_service_init() deve ser chamado após todos os subsistemas já iniciados.
 */
esp_err_t jig_service_init(void);

#ifdef __cplusplus
}
#endif
