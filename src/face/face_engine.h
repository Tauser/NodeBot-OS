#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "face_params.h"   /* face_params_t — definida em src/models */

/*
 * Inicializa double-buffer em PSRAM (2× sprite 320×240 RGB565 ≈ 300 KB).
 * Deve ser chamado APÓS display_init().
 * Idempotente: chamadas repetidas não recriam os buffers.
 */
void face_engine_init(void);

/*
 * Cria FaceRenderTask no Core 1, prioridade 20, loop 50 ms (20 Hz alvo).
 * Deve ser chamado APÓS face_engine_init().
 * Idempotente: ignora chamadas repetidas se a task já estiver rodando.
 */
void face_engine_start_task(void);

/*
 * Atualiza parâmetros de expressão de forma thread-safe (spinlock).
 * Pode ser chamado de qualquer task ou core.
 */
void face_engine_apply_params(const face_params_t *p);

/*
 * Copia a expressão-alvo atual (_dst) para *out. Thread-safe.
 * Útil para debug, idle variations e outros sistemas que precisem inspecionar
 * a face base atualmente selecionada.
 */
void face_engine_get_target(face_params_t *out);

/*
 * Atualiza a posição de olhar do GazeService.
 * x, y em [-0.8, 0.8]; positivo = direita / baixo.
 * Thread-safe (spinlock). Aplicado como offset no próximo frame.
 */
void face_engine_set_gaze(float x, float y);

/*
 * Override visual de blink em [0,1].
 * 0 = sem blink, 1 = olho totalmente fechado.
 * Não altera a expressão-alvo salva; atua apenas no frame renderizado.
 */
void face_engine_set_blink(float amount);

/*
 * Override visual de blink separado por olho.
 * left/right em [0,1].
 */
void face_engine_set_blink_pair(float left, float right);

/*
 * Ativa/desativa indicador de escuta (mic piscando no canto da tela).
 * Quando active=true, desenha um ícone de mic piscando a 2 Hz no canto
 * inferior-direito até ser desativado.
 */
void face_engine_set_listening(bool active);

#ifdef __cplusplus
}
#endif
