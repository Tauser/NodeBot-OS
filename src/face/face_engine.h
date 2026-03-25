#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "face_params.h"   /* face_params_t — definida em src/models */

/*
 * Inicializa double-buffer em PSRAM (2× sprite 240×320 RGB565 ≈ 300 KB).
 * Deve ser chamado APÓS display_init().
 */
void face_engine_init(void);

/*
 * Cria FaceRenderTask no Core 1, prioridade 20, loop 50 ms (20 Hz alvo).
 * Deve ser chamado APÓS face_engine_init().
 */
void face_engine_start_task(void);

/*
 * Atualiza parâmetros de expressão de forma thread-safe (spinlock).
 * Pode ser chamado de qualquer task ou core.
 */
void face_engine_apply_params(const face_params_t *p);

#ifdef __cplusplus
}
#endif
