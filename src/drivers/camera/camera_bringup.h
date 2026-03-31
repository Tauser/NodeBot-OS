#pragma once

/*
 * camera_bringup — bring-up mínimo da OV2640 (E08A)
 *
 * Uso exclusivo de validação de hardware.
 * Não usar em produção — use camera_service (E36) para isso.
 *
 * Sequência obrigatória (Regra R1):
 *   imu_init()  →  camera_bringup_init()
 *
 * ⚠ CONFLITO DE BARRAMENTO I2C
 *   O IMU usa a new IDF I2C master API (i2c_new_master_bus).
 *   A lib esp32-camera usa internamente o SCCB protocol nos GPIOs 4/5.
 *   Para evitar conflito: chamar camera_bringup_init() ANTES de imu_init(),
 *   ou garantir que o bus I2C_NUM_0 não esteja em uso no momento da init.
 *
 * ⚠ NÃO acessar o barramento I2C (IMU, MAX17048, bq25185) durante
 *   uma captura de frame — SCCB e I2C compartilham os GPIOs 4/5.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Inicializa a câmera OV2640 onboard.
 *
 * Configura: grayscale, FRAMESIZE_QQVGA (160×120), fb_count=1,
 *   CAMERA_GRAB_WHEN_EMPTY, xclk=10 MHz.
 *
 * Loga claramente "camera_bringup init ok" ou a causa da falha.
 */
esp_err_t camera_bringup_init(void);

/*
 * Captura um frame e loga width, height, len e timestamp.
 *
 * Retorna ESP_OK se fb != NULL.
 * Loga aviso se frame for nulo.
 */
esp_err_t camera_bringup_capture_once(void);

/*
 * Executa N capturas sequenciais e verifica que todas retornam fb != NULL.
 *
 * Loga resultado de cada captura e resumo ao final.
 * delay_ms: intervalo entre capturas (sugerido: 50 ms).
 *
 * Retorna ESP_OK se todas as capturas forem bem-sucedidas.
 */
esp_err_t camera_bringup_capture_n(uint8_t n, uint32_t delay_ms);

/*
 * Retorna true se camera_bringup_init() concluiu com sucesso.
 */
bool camera_bringup_is_ready(void);

#ifdef __cplusplus
}
#endif
