#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * brownout_handler — E25A.
 *
 * Registra um shutdown handler via esp_register_shutdown_handler() que,
 * na detecção de brownout ou reset inesperado:
 *   1. Grava "unclean_boot" = 1 em NVS.
 *   2. Incrementa "crash_cnt" em NVS.
 *   3. Chama log_flush_now() para preservar o buffer de log.
 *
 * Chamar uma única vez no boot, antes de iniciar tasks.
 */
void brownout_handler_init(void);

#ifdef __cplusplus
}
#endif
