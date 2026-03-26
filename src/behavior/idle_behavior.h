#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * idle_behavior — Idle Behavior Engine (E22 rev2).
 *
 * Tier 2 — behaviors ocasionais com timer independente:
 *   look_side     (8–20 s)    gaze lateral
 *   look_up       (15–35 s)   gaze para cima
 *   squint_think  (12–25 s)   olhos semicerram
 *   slow_blink    (10–20 s)   energy < 0.5
 *   double_blink  (20–40 s)   energy >= 0.5
 *   slight_smile  (20–40 s)   valence > 0
 *
 * Tier 3 — animações raras com cooldown independente:
 *   yawn    (5–15 min)  energy < 0.5
 *   sneeze  (10–30 min)
 *   hiccup  (8–20 min)
 *
 * Cria IdleTask (Core 1, pri 3, 10 Hz).
 */

void idle_behavior_init(void);

/*
 * Dispara bocejo imediatamente, respeitando cooldown de 5 min.
 * Bloqueia a task chamadora ~7 s durante a animação.
 * Suprime blinks automáticos durante a animação.
 */
void idle_behavior_trigger_yawn(void);

#ifdef __cplusplus
}
#endif
