#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── IDs de som ────────────────────────────────────────────────────── */
typedef enum {
    SOUND_BEEP_ACK      = 0,   /* confirmação genérica            */
    SOUND_DING_NOTIF    = 1,   /* notificação / atenção           */
    SOUND_WHOOSH_ACTIVATE = 2, /* ativação / wake                 */
    SOUND_CLICK_TOUCH   = 3,   /* feedback de toque               */
    SOUND_ERROR_TONE    = 4,   /* erro / falha                    */
    SOUND_COUNT         = 5,
} sound_id_t;

/**
 * Inicializa o subsistema de feedback sonoro:
 *  - Tenta carregar cada som do SD (/sdcard/sounds/<nome>.pcm) para PSRAM
 *  - Sons ausentes no SD usam fallback em flash (apenas BEEP_ACK garantido)
 *  - Cria AudioPlaybackTask (Core 1, prioridade 18)
 *
 * Deve ser chamado APÓS audio_init() e sd_init().
 */
esp_err_t audio_feedback_init(void);

/**
 * Enfileira um som para reprodução. Nunca bloqueia (< 1 ms).
 * Se a fila estiver cheia, o comando é descartado silenciosamente.
 *
 * @return ESP_OK            enfileirado com sucesso
 *         ESP_ERR_NO_MEM    fila cheia
 *         ESP_ERR_INVALID_ARG  id inválido
 */
esp_err_t audio_feedback_play(sound_id_t id);

/**
 * Retorna true se a AudioPlaybackTask está reproduzindo um som.
 */
bool audio_feedback_is_playing(void);

#ifdef __cplusplus
}
#endif
