#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Dimensões do ring buffer ──────────────────────────────────────── */
#define AUDIO_BLOCK_SAMPLES  512u    /* amostras por bloco (32 ms @ 16 kHz)  */
#define AUDIO_RING_BLOCKS    80u     /* blocos totais  ≈ 2,56 s de áudio     */

/**
 * Inicializa o driver de captura:
 *  - Aloca ring buffer de AUDIO_RING_BLOCKS × AUDIO_BLOCK_SAMPLES int16_t em PSRAM
 *  - Cria AudioCaptureTask (Core 0, prioridade 14)
 *
 * Deve ser chamado APÓS audio_init() e event_bus_init().
 */
void audio_capture_init(void);

/**
 * Copia o bloco mais recente (AUDIO_BLOCK_SAMPLES amostras) para `out`.
 * Retorna true se um bloco válido estava disponível, false caso contrário.
 * Thread-safe.
 */
bool audio_capture_get_latest(int16_t *out);

#ifdef __cplusplus
}
#endif
