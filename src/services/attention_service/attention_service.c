#include "attention_service.h"
#include "event_bus.h"
#include "gaze_service.h"     /* gaze_service_set_target() */
#include "touch_service.h"    /* touch_event_t             */
#include "touch_driver.h"     /* touch_zone_t TOUCH_ZONE_* */
#include "esp_log.h"

#define TAG "attn"

/*
 * Mapeamento touch_zone_t → (gaze_x, gaze_y).
 * Índice = zone_id conforme touch_zone_t.
 */
static const struct { float x; float y; } k_zone_gaze[] = {
    [TOUCH_ZONE_BASE] = {  0.0f, +0.6f },   /* base: olhar para baixo */
    [TOUCH_ZONE_TOP]  = {  0.0f, -0.6f },   /* topo: olhar para cima  */
    [TOUCH_ZONE_LEFT] = { -0.6f,  0.0f },   /* lado: olhar para esquerda */
};
#define ZONE_GAZE_COUNT  (sizeof(k_zone_gaze) / sizeof(k_zone_gaze[0]))

/* ── callbacks ────────────────────────────────────────────────────────── */

static void on_touch(uint16_t type, void *payload)
{
    (void)type;
    if (!payload) return;
    const touch_event_t *ev = (const touch_event_t *)payload;
    if (ev->zone_id >= ZONE_GAZE_COUNT) return;

    gaze_service_set_target(k_zone_gaze[ev->zone_id].x,
                            k_zone_gaze[ev->zone_id].y, 0);
}

static void on_voice(uint16_t type, void *payload)
{
    (void)type;
    if (!payload) return;
    const uint8_t *active = (const uint8_t *)payload;
    if (*active) {
        /* voz detectada: olhar levemente para frente/cima */
        gaze_service_set_target(0.0f, -0.2f, 0);
    }
}

static void on_motion(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    /* shake ou tilt: olhar para frente */
    gaze_service_set_target(0.0f, -0.3f, 0);
}

/* ── init ─────────────────────────────────────────────────────────────── */

esp_err_t attention_service_init(void)
{
    event_bus_subscribe(EVT_TOUCH_PRESS,   on_touch);
    event_bus_subscribe(EVT_VOICE_ACTIVITY, on_voice);
    event_bus_subscribe(EVT_IMU_SHAKE,     on_motion);
    event_bus_subscribe(EVT_IMU_TILT,      on_motion);

    ESP_LOGI(TAG, "init ok");
    return ESP_OK;
}
