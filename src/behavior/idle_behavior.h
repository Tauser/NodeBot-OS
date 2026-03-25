#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * idle_behavior — variações de vida ociosa do robô (E22).
 *
 * Cria IdleTask (Core 0, pri 3) que, a cada rand(20–40 s):
 *   (a) slight_smile   — sorriso sutil por 3 s
 *   (b) pensive        — olhar levemente para cima (gaze_y=-0.2) por 3 s
 *   (c) double_blink   — piscar duplo rápido
 *
 * Bocejo automático verificado a cada 60 s:
 *   energy < 0.4 AND last_interaction > 5 min → 40% chance de bocejo.
 *   Cooldown: 8 minutos entre bocejos.
 */
void idle_behavior_init(void);

/*
 * Dispara bocejo imediatamente (respeitando cooldown de 8 min).
 * Bloqueia a task chamadora por ~6 s enquanto a animação roda.
 * Suprime blinks automáticos durante a animação.
 */
void idle_behavior_trigger_yawn(void);

#ifdef __cplusplus
}
#endif
