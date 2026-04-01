#pragma once
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * FactoryReset — E40
 *
 * Ativação: 3 toques em ZONE_TOP dentro de 3s → inicia janela de confirmação.
 * Janela de confirmação: 5s com LED âmbar piscando.
 *   - Toque adicional durante a janela → cancela.
 *   - Expiração → apaga NVS + diretórios SD + reinicia.
 *
 * Estados de LED:
 *   Confirmação (5s) : âmbar piscando
 *   Apagamento       : vermelho fixo
 *   Reinício         : verde (via LED_STATE_NORMAL antes do restart)
 *
 * factory_reset_init() deve ser chamado após event_bus_init() e ws2812_init().
 */
esp_err_t factory_reset_init(void);

/* Força o factory reset imediatamente (sem confirmação por toque).
 * Usar apenas em contextos de teste / jig. Não retorna. */
void factory_reset_execute(void);

/* Retorna true se a janela de confirmação estiver ativa. */
bool factory_reset_is_confirming(void);

#ifdef __cplusplus
}
#endif
