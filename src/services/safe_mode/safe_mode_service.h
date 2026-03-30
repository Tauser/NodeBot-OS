#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Verifica e atualiza o boot_count em NVS.
 * Deve ser chamado no início do boot, após config_manager_init().
 *
 * Comportamento:
 *   - Lê e incrementa "boot_count" no namespace "nb_safety"
 *   - Se boot_count >= 3: ativa safe mode (safe_mode_is_active() → true)
 *   - Loga o estado do flag "unclean" do boot anterior e limpa-o
 *
 * Após chamar, use safe_mode_is_active() para aplicar restrições:
 *   - LED: LED_STATE_DEGRADED
 *   - Face: NEUTRAL (via BehaviorEngine ao receber EVT_SYS_ERROR)
 *   - Behavior: apenas diagnóstico serial habilitado
 */
esp_err_t safe_mode_check(void);

/**
 * Retorna true se safe mode está ativo neste boot.
 * Thread-safe (leitura atômica de flag estático).
 */
bool safe_mode_is_active(void);

/**
 * Inicia timer de 60s: se expirar sem reboot, zera boot_count em NVS.
 * Chamar ao final do boot, após todos os subsistemas inicializados.
 * Não chamar se safe mode estiver ativo.
 */
esp_err_t safe_mode_start_stable_timer(void);

#ifdef __cplusplus
}
#endif
