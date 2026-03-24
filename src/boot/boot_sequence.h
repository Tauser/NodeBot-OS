#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Sequência de boot do NodeBot — inicializa todos os subsistemas em ordem.
 *
 * Ordem de inicialização:
 *   1. HAL          — validação da configuração de pinos
 *   2. ConfigManager — NVS, CRC, defaults
 *   3. StorageManager — SD card / FAT
 *   4. LogManager    — buffer PSRAM, task de flush (habilita log_write)
 *   5. Drivers       — display, IMU, touch, LEDs, áudio
 *   6. EventBus      — filas de prioridade
 *   7. PowerManager  — gestão de energia (stub)
 *
 * Comportamento em erro:
 *   - Cada passo é logado via ESP_LOGI (sempre) e log_write (após passo 4).
 *   - Falhas em drivers individuais (passo 5) não abortam a sequência.
 *   - Retorna o primeiro erro encontrado, ou ESP_OK se tudo OK.
 */
esp_err_t app_boot(void);

#ifdef __cplusplus
}
#endif
