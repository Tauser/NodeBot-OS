#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * behavior_engine — BehaviorLoopTask + FSM de alto nível (E33).
 *
 * FSM de estados:
 *   SLEEP      → robô inativo, energia < 0.15
 *   IDLE       → aguardando, sem presença ou atenção relevante
 *   ENGAGED    → pessoa presente ou atenção > 0.5
 *   TALKING    → wake word detectada, atenção = 1.0
 *   SAFE_MODE  → acionado por EVT_SERVO_BLOCKED, dura 5 s
 *
 * Subscrições de evento:
 *   EVT_TOUCH_DETECTED  → arousal + 0.3
 *   EVT_WAKE_WORD       → attention = 1.0
 *   EVT_SYS_LOWBAT      → energy − 0.2
 *   EVT_SERVO_BLOCKED   → entra em SAFE_MODE
 *
 * Cria BehaviorLoopTask (Core 1, pri 12, tick 100 ms).
 */

typedef enum {
    BSTATE_SLEEP = 0,
    BSTATE_IDLE,
    BSTATE_ENGAGED,
    BSTATE_TALKING,
    BSTATE_SAFE_MODE,
    BSTATE_COUNT
} behavior_state_t;

/* Inicia engine e BehaviorLoopTask. Deve ser chamado após event_bus_init(). */
void behavior_engine_init(void);

/* Retorna o estado FSM atual (leitura sem lock — snapshot). */
behavior_state_t behavior_engine_get_state(void);

/* Nome ASCII do estado (nunca retorna NULL). */
const char *behavior_state_name(behavior_state_t s);

#ifdef __cplusplus
}
#endif
