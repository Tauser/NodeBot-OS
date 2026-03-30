#include "gesture_service.h"

#include "event_bus.h"
#include "motion_safety_service.h"

#include <string.h>

typedef struct {
    bool active;
    gesture_command_t cmd;
    uint32_t started_at_ms;
    uint32_t expires_at_ms;
} gesture_slot_t;

static gesture_slot_t s_active;
static bool s_subscribed = false;

static bool time_reached(uint32_t now_ms, uint32_t target_ms)
{
    return (int32_t)(now_ms - target_ms) >= 0;
}

static uint32_t gesture_total_duration_ms(const gesture_command_t *cmd)
{
    return cmd->duration_ms + cmd->hold_ms;
}

static bool gesture_can_preempt(const gesture_command_t *incoming)
{
    if (!s_active.active) {
        return true;
    }

    if (incoming->priority > s_active.cmd.priority) {
        return true;
    }

    if (incoming->priority < s_active.cmd.priority) {
        return false;
    }

    return s_active.cmd.interruptible;
}

static void on_servo_blocked(uint16_t type, void *payload)
{
    (void)type;
    (void)payload;
    gesture_service_cancel();
}

void gesture_service_init(void)
{
    memset(&s_active, 0, sizeof(s_active));

    if (!s_subscribed) {
        event_bus_subscribe(EVT_SERVO_BLOCKED, on_servo_blocked);
        s_subscribed = true;
    }
}

bool gesture_service_submit(const gesture_command_t *cmd, uint32_t now_ms)
{
    if (cmd == NULL) {
        return false;
    }

    if (!motion_safety_is_safe()) {
        return false;
    }

    if (!gesture_can_preempt(cmd)) {
        return false;
    }

    s_active.active = true;
    s_active.cmd = *cmd;
    s_active.started_at_ms = now_ms;
    s_active.expires_at_ms = now_ms + gesture_total_duration_ms(cmd);
    return true;
}

void gesture_service_cancel(void)
{
    memset(&s_active, 0, sizeof(s_active));
}

void gesture_service_tick(uint32_t now_ms)
{
    if (!s_active.active) {
        return;
    }

    if (!motion_safety_is_safe() || time_reached(now_ms, s_active.expires_at_ms)) {
        gesture_service_cancel();
    }
}
