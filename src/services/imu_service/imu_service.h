#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Payload publicado em EVT_IMU_SHAKE e EVT_IMU_TILT.
 * SHAKE: value = variância de magnitude (mg²)
 * TILT:  value = ângulo de inclinação (graus)
 */
typedef struct {
    float value;
} imu_motion_event_t;

/*
 * Inicializa o IMU e cria a task de monitoramento.
 * Core 1, prioridade 7, poll 50ms.
 * Detecta FALL, SHAKE e TILT; FALL chama motion_safety_emergency_stop().
 */
esp_err_t imu_service_init(void);

/* true se ângulo de inclinação atual < 15°. */
bool  imu_service_is_upright(void);

/* Retorna o ângulo de inclinação atual em graus (0 = horizontal). */
float imu_service_get_tilt_deg(void);

#ifdef __cplusplus
}
#endif
