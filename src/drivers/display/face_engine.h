#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Parâmetros de expressão facial.
 * Extensível sem quebrar ABI C — adicionar campos no final.
 */
typedef struct {
    uint8_t eye_open;     /* 0–100: 0=fechado, 100=aberto */
    uint8_t mouth_curve;  /* 0–100: 50=neutro, >50=sorriso, <50=triste */
    uint8_t blink;        /* 1=piscada em andamento */
} face_params_t;

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
