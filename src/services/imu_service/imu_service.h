#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * imu_service — E27.
 *
 * IMUTask (Core 1, pri 7, tick 50 ms):
 *
 *   FALL  : |accel| < 200 mg por > 100 ms
 *             → motion_safety_emergency_stop() + EVT_MOTION_DETECTED(FALL)
 *
 *   SHAKE : variância total (var_x + var_y + var_z) de janela 500 ms > 200 mg²
 *             → EVT_MOTION_DETECTED(SHAKE, intensity)
 *
 *   TILT  : ângulo da vertical > 40° por > 2 s
 *             → EVT_MOTION_DETECTED(TILT, angle_deg)
 *             re-arma quando retorna a < 40°.
 *
 * Chamar após imu_init(), event_bus_init() e motion_safety_init().
 */

/* Sub-tipos de EVT_MOTION_DETECTED */
typedef enum {
    MOTION_FALL  = 0,
    MOTION_SHAKE = 1,
    MOTION_TILT  = 2,
} motion_type_t;

/* Payload de EVT_MOTION_DETECTED */
typedef struct {
    motion_type_t type;
    float         value;   /* FALL: 0 | SHAKE: sqrt(variância total) mg | TILT: ângulo em graus */
} motion_event_t;

/* Inicializa e cria IMUTask. */
void imu_service_init(void);

/* Retorna true se o robô está na vertical (tilt < 40°). Thread-safe. */
bool  imu_service_is_upright(void);

/* Retorna o ângulo atual em relação à vertical (graus). Thread-safe. */
float imu_service_get_tilt_deg(void);

#ifdef __cplusplus
}
#endif
