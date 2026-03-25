#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Namespace NVS e chave de CRC — expostos para testes */
#define CONFIG_NAMESPACE  "nodebot_cfg"
#define CONFIG_CRC_KEY    "crc32"

/**
 * Inicializa o ConfigManager.
 *
 * Chama nvs_flash_init(), abre o namespace, lê o CRC armazenado e
 * o compara com o CRC calculado sobre todos os valores definidos em
 * nvs_defaults.h (ordem fixa).
 *
 * Caso o namespace esteja vazio ou o CRC não bater: executa factory reset
 * e carrega os defaults automaticamente.
 *
 * Seguro chamar múltiplas vezes (idempotente após primeiro init).
 */
esp_err_t config_manager_init(void);

/**
 * Lê uma chave int32 do NVS.
 * Retorna default_val se a chave não existir ou se houver erro de I/O.
 */
int32_t config_get_int(const char *key, int32_t default_val);

/**
 * Grava uma chave int32 no NVS.
 *
 * Write-only-if-changed: não acessa a flash se o valor já for igual.
 * Quando há mudança: grava, recalcula o CRC32 e chama nvs_commit().
 *
 * Nota: apenas chaves definidas em nvs_defaults.h são cobertas pelo CRC.
 *
 * @return ESP_OK            valor gravado (ou sem mudança)
 *         ESP_ERR_NVS_*     erro de flash
 */
esp_err_t config_set_int(const char *key, int32_t val);

/**
 * Apaga todo o namespace NVS e grava os defaults de nvs_defaults.h.
 * Recalcula e armazena o CRC após a gravação.
 */
esp_err_t config_factory_reset(void);

/**
 * Retorna o valor de "schema_ver" armazenado no NVS.
 * Retorna 0 se não encontrado.
 */
uint16_t config_get_schema_version(void);

#ifdef __cplusplus
}
#endif
