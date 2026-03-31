#pragma once

/*
 * i2c_bus — singleton do barramento I2C compartilhado (I2C_NUM_0)
 *
 * Todos os periféricos I2C/SCCB da placa compartilham GPIO4 (SDA) e GPIO5 (SCL):
 *   - IMU     (MPU-6050 / ICM-42688)  addr 0x68
 *   - OV2640  (SCCB)                  addr 0x3C/0x78
 *   - MAX17048 (fuel gauge)            addr 0x36
 *   - bq25185 (carregador)             addr 0x6B
 *
 * Regras de uso:
 *   1. Chamar i2c_bus_init() UMA VEZ, antes de qualquer driver I2C.
 *   2. Cada driver adiciona seus devices via i2c_bus_add_device().
 *   3. NÃO chamar i2c_new_master_bus() fora deste módulo.
 *   4. NÃO acessar I2C durante captura de frame OV2640 (Regra R2 do hal_init.h).
 */

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Inicializa o barramento I2C_NUM_0 (SDA=GPIO4, SCL=GPIO5, 400 kHz).
 * Idempotente — chamadas adicionais retornam ESP_OK sem reinicializar.
 */
esp_err_t i2c_bus_init(void);

/*
 * Retorna o handle do bus. Chamar somente após i2c_bus_init().
 */
i2c_master_bus_handle_t i2c_bus_get_handle(void);

/*
 * Adiciona um device ao barramento e retorna o handle do device.
 * Wrapper de i2c_master_bus_add_device() com configuração padrão.
 */
esp_err_t i2c_bus_add_device(uint16_t addr_7bit,
                              i2c_master_dev_handle_t *out_dev);

#ifdef __cplusplus
}
#endif
