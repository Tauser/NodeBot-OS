#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * motion_safety_service — E23.
 *
 * MotionSafetyTask (Core 1, pri 22, tick 5 ms):
 *   (1) Lê corrente de cada servo; >800 mA por >80 ms acumulados →
 *       desabilita torque + publica EVT_SERVO_BLOCKED.
 *   (2) Heartbeat watchdog: sem EVT_SERVICE_HEARTBEAT por >500 ms →
 *       desabilita torque (falha de watchdog, sem evento publicado).
 *
 * Sem malloc, sem delay(), sem SD, sem I2C.
 */

/* Payload de EVT_SERVO_BLOCKED */
typedef struct {
    uint8_t  servo_id;    /* 0=pan, 1=tilt                  */
    uint16_t current_ma;  /* corrente no momento do bloqueio */
} servo_blocked_event_t;

/* Inicializa e cria MotionSafetyTask. Chamar após event_bus_init(). */
void motion_safety_init(void);

/*
 * Alimenta o heartbeat watchdog.
 * Chamar periodicamente (< 500 ms) do BehaviorLoop ou similar.
 * Thread-safe.
 */
void motion_safety_feed_heartbeat(void);

/*
 * Para todos os servos imediatamente e marca o sistema como não seguro.
 * Pode ser chamado de qualquer task. Thread-safe.
 */
void motion_safety_emergency_stop(void);

/*
 * Retorna true se os servos podem ser operados com segurança.
 * false após overcurrent, heartbeat timeout ou emergency_stop.
 */
bool motion_safety_is_safe(void);

#ifdef __cplusplus
}
#endif
