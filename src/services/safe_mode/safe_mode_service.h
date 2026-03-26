#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * safe_mode_service — E25B.
 *
 * Detecta boots consecutivos não estáveis e ativa safe mode
 * automaticamente após CONFIG_SAFE_MODE_BOOT_THRESH boots sem
 * 60 s de operação limpa.
 *
 * Sequência no boot:
 *   1. Lê "unclean_boot" — se 1, registra e limpa.
 *   2. Incrementa "boot_cnt".
 *   3. Se boot_cnt >= 3 → ativa safe mode imediatamente.
 *   4. Caso contrário, agenda timer de 60 s; ao expirar zera boot_cnt.
 *
 * Safe mode:
 *   - Face → EMOTION_NEUTRAL.
 *   - LEDs  → LED_STATE_DEGRADED (âmbar).
 *   - Diagnósticos seriais habilitados via log.
 *   - Permanece até reset manual.
 *
 * Chamar após ws2812_init(), config_manager_init() e face_engine_init().
 */

#define SAFE_MODE_BOOT_THRESH  3    /* boots consecutivos para ativar */
#define SAFE_MODE_STABLE_S     60   /* segundos sem crash para resetar boot_cnt */

void safe_mode_check(void);
bool safe_mode_is_active(void);

#ifdef __cplusplus
}
#endif
