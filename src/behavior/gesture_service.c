#include "gesture_service.h"

#include "event_bus.h"
#include "motion_safety_service.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "gesture";

/*
 * Stub fraco para o driver SCS0009 (substituído quando E04 for integrado).
 * pos_deg: ângulo alvo em graus (0 = centro).
 */
__attribute__((weak)) void scs0009_write_pos_deg(uint8_t id, float pos_deg)
{
    (void)id; (void)pos_deg;
}

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
    if (!s_active.active) return;

    if (!motion_safety_is_safe()) {
        ESP_LOGW(TAG, "safety BLOCKED — cancelando gesto");
        scs0009_write_pos_deg(0, 0.0f);   /* retorna ao centro */
        gesture_service_cancel();
        return;
    }

    if (time_reached(now_ms, s_active.expires_at_ms)) {
        scs0009_write_pos_deg(0, 0.0f);
        gesture_service_cancel();
        return;
    }

    /* Interpola posição: go → hold → return */
    uint32_t elapsed   = now_ms - s_active.started_at_ms;
    uint32_t dur_go    = s_active.cmd.duration_ms;
    uint32_t dur_hold  = s_active.cmd.hold_ms;
    float    target    = s_active.cmd.pan_delta_deg;
    float    pos       = 0.0f;

    if (elapsed <= dur_go && dur_go > 0u) {
        pos = target * (float)elapsed / (float)dur_go;
    } else if (elapsed <= dur_go + dur_hold) {
        pos = target;
    } else {
        uint32_t ret_elapsed = elapsed - dur_go - dur_hold;
        uint32_t ret_total   = (s_active.cmd.duration_ms / 2u);
        if (ret_total == 0u) ret_total = 200u;
        float t = (float)ret_elapsed / (float)ret_total;
        if (t > 1.0f) t = 1.0f;
        pos = target * (1.0f - t);
    }

    scs0009_write_pos_deg(0, pos);
}

/* ── Gestos nomeados ────────────────────────────────────────────────────── */

bool gesture_perform(gesture_id_t id)
{
    if (!motion_safety_is_safe()) {
        ESP_LOGW(TAG, "gesture_perform(%d): safety BLOCKED", (int)id);
        return false;
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);

    gesture_command_t cmd = {
        .target       = GESTURE_TARGET_HEAD,
        .tilt_delta_deg = 0.0f,
        .source       = GESTURE_SOURCE_BEHAVIOR,
        .interruptible = true,
    };

    switch (id) {
        case GESTURE_GREET:
            cmd.pan_delta_deg = 30.0f;
            cmd.duration_ms   = 200u;   /* vai em 200ms, volta em 200ms */
            cmd.hold_ms       = 0u;
            cmd.priority      = GESTURE_PRIORITY_NORMAL;
            break;
        case GESTURE_ATTENTIVE:
            cmd.pan_delta_deg = 10.0f;
            cmd.duration_ms   = 300u;
            cmd.hold_ms       = 2000u;
            cmd.priority      = GESTURE_PRIORITY_LOW;
            break;
        case GESTURE_REST:
            cmd.pan_delta_deg = 0.0f;
            cmd.duration_ms   = 500u;
            cmd.hold_ms       = 0u;
            cmd.priority      = GESTURE_PRIORITY_HIGH;
            cmd.interruptible = false;
            break;
        default:
            return false;
    }

    bool ok = gesture_service_submit(&cmd, now_ms);
    if (!ok) ESP_LOGD(TAG, "gesture %d rejeitado (prioridade)", (int)id);
    return ok;
}
