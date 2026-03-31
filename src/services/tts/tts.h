#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Frases pré-gravadas ────────────────────────────────────────────────── */
typedef enum {
    PHRASE_NOT_UNDERSTOOD    = 0,   /* "não entendi"          SD: nao_entendi.pcm    */
    PHRASE_TIMEOUT_LISTENING = 1,   /* "pode repetir?"        SD: pode_repetir.pcm   */
    PHRASE_OK                = 2,   /* "ok"                   SD: ok.pcm             */
    PHRASE_SLEEP             = 3,   /* "vou dormir"           SD: vou_dormir.pcm     */
    PHRASE_WAKE              = 4,   /* "estou acordado"       SD: estou_acordado.pcm */
    PHRASE_SILENCE           = 5,   /* "ok, silêncio"         SD: silencio.pcm       */
    PHRASE_HOW_ARE_YOU       = 6,   /* "estou bem, obrigado"  SD: estou_bem.pcm      */
    PHRASE_LOOK_AT_ME        = 7,   /* "te vejo"              SD: te_vejo.pcm        */
    PHRASE_VOLUME_UP         = 8,   /* "volume aumentado"     SD: volume_alto.pcm    */
    PHRASE_VOLUME_DOWN       = 9,   /* "volume reduzido"      SD: volume_baixo.pcm   */
    PHRASE_YES               = 10,  /* "sim"                  SD: sim.pcm            */
    PHRASE_NO                = 11,  /* "não"                  SD: nao.pcm            */
    PHRASE_CANCEL            = 12,  /* "cancelando"           SD: cancelando.pcm     */
    PHRASE_COUNT
} phrase_id_t;

/* ── Templates dinâmicos ────────────────────────────────────────────────── */
typedef enum {
    TPL_HOUR = 0,   /* "são X horas" — val = hora 0..23; SD: hora_0.pcm .. hora_23.pcm */
    TPL_COUNT
} tts_template_t;

/*
 * Inicializa o TTS.
 *
 * Tenta carregar frases de /sdcard/tts/ *.pcm (aceita .wav) para PSRAM.
 * Frases ausentes usam SOUND_BEEP_ACK como fallback silencioso.
 * Cria tts_task (Core 1, P16).
 *
 * Deve ser chamado após audio_init(), sd_init() e event_bus_init().
 */
esp_err_t tts_init(void);

/*
 * Enfileira frase para reprodução assíncrona.
 * NUNCA bloqueia — retorna em < 1 ms.
 * Descarta silenciosamente se fila cheia.
 * Publica EVT_TTS_DONE ao término.
 */
void tts_play_phrase(phrase_id_t id);

/*
 * Enfileira template dinâmico para reprodução assíncrona.
 * val: valor numérico inserido no template (ex: TPL_HOUR, val = 14 → "são 14 horas").
 * Publica EVT_TTS_DONE ao término.
 */
void tts_play_dynamic(tts_template_t tpl, int val);

#ifdef __cplusplus
}
#endif
