#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── FSM de alto nível do BehaviorEngine ──────────────────────────────── */
typedef enum {
    ENGINE_SLEEP     = 0,   /* baixa energia, sem interação          */
    ENGINE_IDLE      = 1,   /* aguardando                            */
    ENGINE_ENGAGED   = 2,   /* recentemente ativado (wake/toque)     */
    ENGINE_TALKING   = 3,   /* em diálogo ativo                      */
    ENGINE_SAFE_MODE = 4,   /* safe mode do sistema ativo            */
} engine_state_t;

/*
 * Inicializa o BehaviorEngine.
 *
 * Inicializa behavior_tree, cria BehaviorLoopTask (Core 1, P12, 100 ms).
 * Subscreve EVT_WAKE_WORD, EVT_TOUCH_PRESS, EVT_INTENT_DETECTED,
 *           EVT_TTS_DONE, EVT_DIALOGUE_STATE_CHANGED, EVT_SYS_LOWBAT.
 *
 * Deve ser chamado por último no boot, após todos os serviços.
 */
esp_err_t behavior_engine_init(void);

/* Retorna o estado FSM corrente. Thread-safe. */
engine_state_t behavior_engine_get_state(void);

#ifdef __cplusplus
}
#endif
