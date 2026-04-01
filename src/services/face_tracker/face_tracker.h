#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * FaceTracker — E43
 *
 * Rastreia o rosto detectado pela câmera movendo o servo de pescoço (pan).
 * Lê presence_detector_get_last() a cada tick (20 Hz, 50 ms).
 * Submete gestos de pan via gesture_service_submit() — toda movimentação
 * passa por motion_safety_is_safe() antes de executar.
 *
 * Comportamento:
 *   - Dead zone ±5% do frame: sem movimento quando rosto está centralizado.
 *   - PID (P + D): delta_deg = Kp×err + Kd×(err - err_prev).
 *   - Velocity limit: máx MAX_STEP_DEG por tick.
 *   - Cooldown 500 ms após fim de diálogo: servo não se move enquanto robô fala.
 *   - Fallback: 5 s sem detecção → retornar ao centro (GESTURE_REST).
 *
 * Publica EVT_FACE_TRACKER_UPDATE (event_bus.h) a cada mudança de estado.
 *
 * Depende de: presence_detector_init(), gesture_service_init(),
 *             motion_safety_init(), event_bus_init().
 */
esp_err_t face_tracker_init(void);

/* Habilita ou desabilita o tracking (ex.: durante animações longas). */
void face_tracker_set_enabled(bool enabled);

/* Payload de EVT_FACE_TRACKER_UPDATE */
typedef struct {
    float   target_deg;       /* ângulo alvo calculado pelo PID       */
    float   current_deg;      /* posição acumulada estimada           */
    bool    tracking_active;  /* true = detectou rosto e está movendo */
} face_tracker_event_t;

#ifdef __cplusplus
}
#endif
