#pragma once
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * OTAManager — E39
 *
 * Fluxo de atualização:
 *   1. Janela de segurança verificada antes de qualquer download.
 *   2. Firmware baixado para a partição inativa via HTTPS.
 *   3. Assinatura ECDSA P-256 verificada ANTES do swap de partição.
 *   4. Se OK: define partição de boot + reinicia.
 *   5. Novo firmware tem 60s para chamar ota_manager_mark_stable().
 *      Se não chamar (crash, watchdog), bootloader faz rollback.
 *
 * Janela de segurança obrigatória (ota_manager_check_and_apply):
 *   battery_pct > 30% AND servos não em emergência AND diálogo inativo.
 *
 * Rollback configurado via sdkconfig.defaults:
 *   CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
 *   CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=n
 */

/*
 * Inicializa o OTAManager.
 * Detecta se o boot atual é de um firmware pendente de verificação e,
 * caso seja, inicia watchdog de 60 s. Deve ser chamado antes do
 * BehaviorEngine, depois do EventBus.
 */
esp_err_t ota_manager_init(void);

/*
 * Verifica a janela de segurança e, se OK, baixa o firmware de `url`,
 * verifica a assinatura ECDSA (arquivo `url + ".sig"`) e, se válida,
 * define a nova partição de boot e reinicia o dispositivo.
 *
 * Bloqueia a tarefa chamadora durante o download (~30 s em 300 kbps).
 * NÃO chamar de tasks de alta prioridade. Chamar de uma task dedicada.
 *
 * Retorna:
 *   ESP_OK              — nunca (reinicia antes de retornar em caso de sucesso)
 *   ESP_ERR_INVALID_STATE — janela de segurança não satisfeita
 *   ESP_ERR_INVALID_ARG   — URL nula
 *   outros              — falha de download, verificação ou escrita
 */
esp_err_t ota_manager_check_and_apply(const char *url);

/*
 * Confirma que o novo firmware está estável.
 * Cancela o watchdog e chama esp_ota_mark_app_valid_cancel_rollback().
 * Chamar após 60 s de operação verificada (gerenciado internamente via
 * ota_manager_send_heartbeat).
 */
void ota_manager_mark_stable(void);

/*
 * Notifica o OTAManager que o sistema está operacional.
 * Chamar do BehaviorLoop a cada tick (~100 ms).
 * Internamente acumula ~60 s e chama mark_stable() automaticamente.
 * No-op se não há firmware pendente de verificação.
 */
void ota_manager_send_heartbeat(void);

/* Retorna true se o boot atual é de firmware pendente de verificação. */
bool ota_manager_is_pending_verify(void);

#ifdef __cplusplus
}
#endif
