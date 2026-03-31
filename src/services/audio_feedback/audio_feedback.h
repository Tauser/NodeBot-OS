#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOUND_BEEP_ACK        = 0,   /* confirmação de ação           */
    SOUND_DING_NOTIF      = 1,   /* notificação                   */
    SOUND_WHOOSH_ACTIVATE = 2,   /* ativação de wake word         */
    SOUND_CLICK_TOUCH     = 3,   /* resposta a toque              */
    SOUND_ERROR_TONE      = 4,   /* erro / não entendeu           */
    SOUND_COUNT                  /* sentinela                     */
} sound_id_t;

/*
 * Inicializa o AudioFeedbackService.
 *
 * Tenta carregar sons do SD (/sdcard/sounds/NOME.pcm) para PSRAM.
 * Sons ausentes ou SD offline: usa beep gerado em software como fallback.
 * Cria AudioPlaybackTask (Core 1, P18).
 *
 * Deve ser chamado após audio_init() e sd_init().
 */
esp_err_t audio_feedback_init(void);

/*
 * Enfileira `id` para reprodução.
 * NUNCA bloqueia — retorna em < 1ms.
 * Se a fila estiver cheia, o comando é descartado silenciosamente.
 */
void audio_feedback_play(sound_id_t id);

#ifdef __cplusplus
}
#endif
