#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Registra o handler de brownout via esp_register_shutdown_handler().
 * Deve ser chamado logo no início do boot, antes de qualquer I/O.
 *
 * Ao disparar, o handler executa em < 5ms:
 *   1. Incrementa "crash_count" no namespace NVS "nb_safety"
 *   2. Seta "unclean" = 1
 *   3. Chama nvs_commit()
 *
 * RESTRIÇÕES do handler: sem SD, sem I2C, sem malloc, sem printf.
 */
esp_err_t brownout_handler_init(void);

#ifdef __cplusplus
}
#endif
