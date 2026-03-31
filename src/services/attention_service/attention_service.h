#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AttentionService — direciona o olhar para o estímulo de maior prioridade.
 *
 * Subscreve EVT_TOUCH_PRESS, EVT_VOICE_ACTIVITY, EVT_IMU_SHAKE, EVT_IMU_TILT.
 * Chama gaze_service_set_target() diretamente no callback (latência < 200 ms).
 *
 * Mapeamento zona → gaze:
 *   TOUCH_ZONE_BASE  (0)  →  (0.0, +0.6)  — baixo
 *   TOUCH_ZONE_TOP   (1)  →  (0.0, -0.6)  — cima
 *   TOUCH_ZONE_LEFT  (2)  →  (-0.6, 0.0)  — esquerda
 *   Voz ativa             →  (0.0, -0.2)  — frente/cima
 *   IMU shake/tilt        →  (0.0, -0.3)  — frente
 *
 * Deve ser chamado após event_bus_init() e gaze_service_init().
 */
esp_err_t attention_service_init(void);

#ifdef __cplusplus
}
#endif
