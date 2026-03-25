#pragma once

/*
 * face_renderer — interface C para o renderizador de olhos Cozmo/EMO.
 * Implementação em face_renderer.cpp (C++/LovyanGFX).
 *
 * Uso típico (C ou C++):
 *   face_renderer_set_sprite(my_sprite);
 *   face_params_t p = FACE_ANGRY;
 *   p.transition_ms = 200;
 *   face_renderer_draw(&p);
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "face_params.h"

/*
 * Registra o sprite de destino (lgfx::LGFX_Sprite* como void*).
 * Deve ser chamado antes de face_renderer_draw().
 */
void face_renderer_set_sprite(void *sprite);

/*
 * Limpa o sprite e renderiza dois olhos independentes conforme *p.
 * Não faz display_push_sprite — responsabilidade do caller.
 */
void face_renderer_draw(const face_params_t *p);

#ifdef __cplusplus
}
#endif
