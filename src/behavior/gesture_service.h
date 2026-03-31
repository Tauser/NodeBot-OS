#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GESTURE_TARGET_HEAD = 0,
} gesture_target_t;

typedef enum {
    GESTURE_PRIORITY_LOW = 0,
    GESTURE_PRIORITY_NORMAL = 1,
    GESTURE_PRIORITY_HIGH = 2,
    GESTURE_PRIORITY_CRITICAL = 3,
} gesture_priority_t;

typedef enum {
    GESTURE_SOURCE_SYSTEM = 0,
    GESTURE_SOURCE_BEHAVIOR = 1,
    GESTURE_SOURCE_TOUCH = 2,
    GESTURE_SOURCE_VOICE = 3,
} gesture_source_t;

typedef struct {
    gesture_target_t target;
    float pan_delta_deg;
    float tilt_delta_deg;
    uint32_t duration_ms;
    uint32_t hold_ms;
    gesture_priority_t priority;
    gesture_source_t source;
    bool interruptible;
} gesture_command_t;

/* ── Gestos nomeados ─────────────────────────────────────────────────── */
typedef enum {
    GESTURE_GREET     = 0,   /* servo0: 0°→30°→0° em 400ms              */
    GESTURE_ATTENTIVE = 1,   /* servo0: 0°→10° em 300ms, hold 2s, volta */
    GESTURE_REST      = 2,   /* todos para 0° em 500ms                   */
    GESTURE_ID_COUNT
} gesture_id_t;

void gesture_service_init(void);
bool gesture_service_submit(const gesture_command_t *cmd, uint32_t now_ms);
void gesture_service_cancel(void);
void gesture_service_tick(uint32_t now_ms);

/*
 * Executa gesto nomeado.
 * Verifica motion_safety_is_safe() — aborta e loga se BLOCKED.
 * Retorna false se segurança bloqueada ou gesto em andamento com prioridade maior.
 */
bool gesture_perform(gesture_id_t id);

#ifdef __cplusplus
}
#endif
