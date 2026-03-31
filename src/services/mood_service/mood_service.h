#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Payload do EVT_MOOD_CHANGED */
typedef struct {
    float valence;  /* [-1, +1]  negativo = triste / positivo = feliz */
    float arousal;  /* [ 0, +1]  baixo = calmo  / alto = animado      */
} mood_event_t;

/*
 * Inicializa o MoodService.
 *
 * Cria MoodTask (Core 1, P9, 1 s/tick) que aplica decay exponencial e
 * publica EVT_MOOD_CHANGED quando |Δvalence| >= 0.05 ou |Δarousal| >= 0.05.
 *
 * Decay:
 *   valence  τ = 4 h (14400 s) — mudança lenta ao longo do dia
 *   arousal  τ = 30 min (1800 s) — mais reativo a eventos recentes
 *
 * Subscreve EVT_TOUCH_PRESS, EVT_WAKE_WORD, EVT_TTS_DONE.
 * Deve ser chamado após event_bus_init().
 */
esp_err_t mood_service_init(void);

/*
 * Aplica impulso imediato de valence e arousal.
 * Valores somados ao estado atual e clampeados para [-1,1] / [0,1].
 * Thread-safe. Publica EVT_MOOD_CHANGED se o limiar for atingido.
 */
void mood_service_boost(float d_valence, float d_arousal);

/* Lê o estado atual. Ponteiro NULL é ignorado. Thread-safe. */
void mood_service_get(float *valence, float *arousal);

#ifdef __cplusplus
}
#endif
