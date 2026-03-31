#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Estados do FSM de diálogo ─────────────────────────────────────────── */
typedef enum {
    DIALOGUE_IDLE       = 0,   /* aguardando wake word                     */
    DIALOGUE_LISTENING  = 1,   /* escutando comando (timer 3 s)            */
    DIALOGUE_PROCESSING = 2,   /* mapeando intent → resposta (timer 5 s)   */
    DIALOGUE_SPEAKING   = 3,   /* reproduzindo resposta TTS                */
} dialogue_state_t;

/*
 * Inicializa o DialogueStateService.
 *
 * Subscreve EVT_WAKE_WORD, EVT_INTENT_DETECTED e EVT_TTS_DONE.
 * Cria timers de segurança: LISTENING 3 s, PROCESSING 5 s.
 * Não cria task própria — FSM é dirigido por callbacks do EventBus.
 *
 * Deve ser chamado após event_bus_init() e tts_init().
 */
esp_err_t dialogue_state_service_init(void);

/* Retorna o estado corrente do FSM. Thread-safe. */
dialogue_state_t dialogue_state_get(void);

#ifdef __cplusplus
}
#endif
