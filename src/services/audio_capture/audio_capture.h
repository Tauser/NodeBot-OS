#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "vad.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_CAPTURE_BLOCK_SAMPLES  512u   /* 32ms @ 16kHz                */
#define AUDIO_CAPTURE_RING_BLOCKS    160u   /* ~5s de buffer em PSRAM      */

/*
 * Inicializa o AudioCaptureService.
 *
 * Aloca ring buffer (~160KB) em PSRAM.
 * Inicializa o VAD (lê threshold do NVS).
 * Cria AudioCaptureTask (Core 0, P14).
 *
 * Deve ser chamado após audio_init() e config_manager_init().
 */
esp_err_t audio_capture_init(void);

/*
 * Lê até `samples` amostras do ring buffer (consumidor, ex: wake word E30).
 * Bloqueia até dados disponíveis ou timeout de 100ms.
 * Retorna o número de amostras copiadas (0 = timeout).
 */
size_t audio_capture_read(int16_t *buf, size_t samples);

#ifdef __cplusplus
}
#endif
