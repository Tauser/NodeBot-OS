#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Resultado do VAD para um bloco de amostras.
 *
 * Payload de EVT_VOICE_ACTIVITY.
 */
typedef struct {
    float energy_db;   /* 20×log10(rms/32767) — negativo; 0 dB = saturação */
    bool  is_speech;   /* true = fala detectada                             */
} vad_event_t;

/*
 * Inicializa o VAD.
 * Lê rms_threshold do NVS (chave "vad_rms_thr"); default 200.
 * Deve ser chamado após config_manager_init().
 */
void vad_init(void);

/*
 * Processa um bloco de amostras PCM mono 16-bit.
 * count deve ser o tamanho do bloco (tipicamente 512).
 * Thread-safe (apenas leitura de estado imutável + volatile gate).
 */
vad_event_t nb_vad_process(const int16_t *samples, size_t count);

/*
 * Suprime detecção de fala por `ms` milissegundos.
 * Chamar quando playback iniciar para evitar eco.
 * Pode ser chamado de qualquer core/task.
 */
void vad_suppress_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
