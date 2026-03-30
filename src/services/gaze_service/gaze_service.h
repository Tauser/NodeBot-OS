#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * gaze_service — gerador de olhar para o robô (E20).
 *
 * Mantém uma posição de olhar normalizada (x, y) em [-0.8, +0.8] e executa:
 *
 *   Saccade (movimentos voluntários):
 *     1. Overshoot = target + (target - current) × 0.12
 *     2. Desloca até overshoot em duration_ms × 0.7  (ease-out)
 *     3. Retorna ao target em 80 ms                   (ease-out)
 *
 *   Idle (deriva aleatória):
 *     A cada rand(2000, 5000) ms gera novo alvo gaussian:
 *       x ~ N(0, 0.3),  y ~ N(0, 0.2),  clamped ±0.8
 *
 * Publicação: EVT_GAZE_UPDATE (EVENT_PRIO_COSMETIC) com payload gaze_event_t.
 *
 * Uso:
 *   gaze_service_init();                   // cria GazeTask (Core 1, pri 10)
 *   gaze_service_set_target(0.5f, -0.3f, 300);  // saccade externo
 *   gaze_service_set_target(0.5f, -0.3f, 0);    // duração automática pela distância
 *   gaze_service_get(&x, &y);             // lê posição atual (thread-safe)
 */

/* Payload do EVT_GAZE_UPDATE */
typedef struct {
    float x;   /* [-0.8, +0.8]  positivo = direita  */
    float y;   /* [-0.8, +0.8]  positivo = baixo    */
} gaze_event_t;

/*
 * Inicializa o serviço e cria GazeTask (Core 1, pri 10, 100 ms/tick).
 * Deve ser chamado após event_bus_init().
 */
void gaze_service_init(void);

/*
 * Dispara uma saccade para (x, y) com duração total aproximada de duration_ms.
 * x e y são clampeados para ±0.8 internamente.
 * Se duration_ms == 0, o serviço deriva a duração pela distância.
 * Durante a saccade, o blink automático é temporariamente suprimido.
 * Thread-safe.
 */
void gaze_service_set_target(float x, float y, uint16_t duration_ms);

/*
 * Copia a posição de olhar atual para *x e *y.
 * Qualquer ponteiro NULL é ignorado. Thread-safe.
 */
void gaze_service_get(float *x, float *y);

#ifdef __cplusplus
}
#endif
