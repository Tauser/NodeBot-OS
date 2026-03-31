#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Contexto passado pelo BehaviorEngine a cada tick ─────────────────── */
typedef struct {
    uint32_t now_ms;
    uint32_t last_wake_word_ms;   /* 0 = nunca          */
    uint32_t last_touch_ms;       /* 0 = nunca          */
    bool     safe_mode_active;
    float    battery_pct;         /* 0.0–100.0          */
} bt_context_t;

typedef enum {
    BT_SUCCESS = 0,   /* nó executou ação                 */
    BT_FAILURE = 1,   /* nenhum nó aplicável              */
} bt_result_t;

/*
 * Inicializa a behavior tree (nenhum malloc — nós são estáticos).
 * Deve ser chamado antes de behavior_tree_evaluate().
 */
esp_err_t behavior_tree_init(void);

/*
 * Avalia a árvore de prioridade, executa a primeira condição verdadeira.
 * Deve completar em < 10 ms (sem I/O, sem bloqueio).
 *
 * Árvore de prioridade (P1 = mais alta):
 *   P1 safety_mode_active  → emotion NEUTRAL (face segura), suprime movimentos
 *   P2 battery < 10%       → emotion SAD (cansado), energy low
 *   P3 wake_word < 5 s     → emotion SURPRISED → NEUTRAL (engajamento)
 *   P4 touch < 2 s         → emotion HAPPY (reação ao toque)
 *   P5 default             → BT_FAILURE (idle_behavior task roda sozinha)
 */
bt_result_t behavior_tree_evaluate(const bt_context_t *ctx);

/*
 * Deriva expressão base do StateVector (EMO_SPEC §3) e aplica em FACE_PRIORITY_MOOD.
 * Chamar do BehaviorEngine a cada 1 s.
 */
void behavior_tree_apply_mood(void);

#ifdef __cplusplus
}
#endif
