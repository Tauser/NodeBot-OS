#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Payload de EVT_VOICE_ACTIVITY */
typedef struct {
    float energy_db;   /* 20 × log10(rms / 32767), dBFS       */
    bool  is_speech;   /* true = fala detectada, false = silêncio */
} voice_activity_event_t;

/**
 * Inicializa o VAD:
 *  - Lê rms_threshold do NVS (namespace "nodebot_vad", chave "rms_thr", default 200)
 */
void vad_init(void);

/**
 * Processa um bloco de amostras PCM 16-bit.
 * Calcula RMS e ZCR; publica EVT_VOICE_ACTIVITY se is_speech muda.
 * Chamado pelo AudioCaptureTask — não chamar de outro contexto.
 *
 * @param samples  Buffer de amostras mono 16-bit
 * @param n        Número de amostras
 */
void vad_process_block(const int16_t *samples, size_t n);

/**
 * Ativa supressão de VAD por `ms` milissegundos.
 * Durante a supressão, EVT_VOICE_ACTIVITY.is_speech é forçado a false
 * (evita feedback durante playback).
 */
void vad_suppress_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
