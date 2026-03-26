#include "touch_service.h"
#include "touch_driver.h"
#include "event_bus.h"
#include "state_vector.h"
#include "emotion_mapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <string.h>

static const char *TAG = "TOUCH_SVC";

/* ── Configuração ─────────────────────────────────────────────────── */
#define TASK_TICK_MS    20u     /* período da task (ms)                   */
#define DEBOUNCE_MS     50u     /* tempo mínimo para confirmar toque (ms) */
#define CAL_SAMPLES     100     /* amostras por zona na calibração        */
#define CAL_MULTIPLIER  1.5f    /* S3: raw sobe ao tocar → thr acima da média */
#define NVS_NS          "nodebot_touch"

/* ── Estado ───────────────────────────────────────────────────────── */
static uint32_t s_threshold[HAL_TOUCH_ZONE_COUNT];

typedef enum { TS_IDLE, TS_DEBOUNCE, TS_ACTIVE } touch_state_t;

typedef struct {
    touch_state_t state;
    uint32_t      press_ms;
} zone_ctx_t;

static zone_ctx_t s_ctx[HAL_TOUCH_ZONE_COUNT];

/* ── Helpers ──────────────────────────────────────────────────────── */

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── NVS ──────────────────────────────────────────────────────────── */

static void nvs_save_thresholds(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    char key[8];
    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        snprintf(key, sizeof(key), "thr_z%d", z);
        nvs_set_u32(h, key, s_threshold[z]);
    }
    nvs_commit(h);
    nvs_close(h);
}

static bool nvs_load_thresholds(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    char key[8];
    bool ok = true;
    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        snprintf(key, sizeof(key), "thr_z%d", z);
        if (nvs_get_u32(h, key, &s_threshold[z]) != ESP_OK) {
            ok = false;
            break;
        }
    }
    nvs_close(h);
    return ok;
}

/* ── Calibração ───────────────────────────────────────────────────── */

void touch_service_calibrate(void)
{
    ESP_LOGI(TAG, "calibrando — mantenha as zonas sem toque...");

    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        uint64_t sum = 0;
        for (int i = 0; i < CAL_SAMPLES; i++) {
            sum += touch_driver_read_raw((touch_zone_t)z);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        const uint32_t mean = (uint32_t)(sum / CAL_SAMPLES);
        s_threshold[z] = (uint32_t)((float)mean * CAL_MULTIPLIER);
        ESP_LOGI(TAG, "zona %d  média=%lu  threshold=%lu",
                 z, (unsigned long)mean, (unsigned long)s_threshold[z]);
    }

    nvs_save_thresholds();
    ESP_LOGI(TAG, "calibração concluída e salva no NVS");
}

/* ── Reação básica (subscriber de EVT_TOUCH_DETECTED) ──────────────── */

static void on_touch_detected(uint16_t type, void *payload)
{
    (void)type;
    const touch_event_t *evt = (const touch_event_t *)payload;

    /* Arousal spike conforme spec E26 */
    g_state.mood_arousal = clampf(g_state.mood_arousal + 0.3f, 0.0f, 1.0f);

    /* Efeitos sociais / valência via state_vector */
    state_vector_on_interaction();

    /* Reação facial */
    emotion_mapper_apply(EMOTION_SURPRISED, 200);

    ESP_LOGD(TAG, "touch z%u  intensity=%.2f  duration=%lums  arousal→%.2f",
             evt->zone_id, evt->intensity,
             (unsigned long)evt->duration_ms, g_state.mood_arousal);
}

/* ── TouchTask ────────────────────────────────────────────────────── */

static void touch_task(void *arg)
{
    (void)arg;
    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TASK_TICK_MS));

        const uint32_t now = now_ms();

        for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
            const uint32_t raw     = touch_driver_read_raw((touch_zone_t)z);
            const bool     touched = (s_threshold[z] > 0u) &&
                                     (raw > s_threshold[z]);
            zone_ctx_t    *ctx     = &s_ctx[z];

            switch (ctx->state) {

                case TS_IDLE:
                    if (touched) {
                        ctx->press_ms = now;
                        ctx->state    = TS_DEBOUNCE;
                    }
                    break;

                case TS_DEBOUNCE:
                    if (!touched) {
                        ctx->state = TS_IDLE;   /* falso positivo */
                    } else if ((now - ctx->press_ms) >= DEBOUNCE_MS) {
                        const float thr_f = (float)s_threshold[z];
                        const float intensity = clampf(
                            ((float)raw - thr_f) / thr_f, 0.0f, 1.0f);

                        touch_event_t evt = {
                            .zone_id     = (uint8_t)z,
                            .intensity   = intensity,
                            .duration_ms = now - ctx->press_ms,
                        };
                        event_bus_publish(EVT_TOUCH_DETECTED,
                                          &evt, sizeof(evt),
                                          EVENT_PRIO_BEHAVIOR);
                        ctx->state = TS_ACTIVE;
                    }
                    break;

                case TS_ACTIVE:
                    if (!touched) {
                        ctx->state = TS_IDLE;
                    }
                    break;
            }
        }
    }
}

/* ── Init ─────────────────────────────────────────────────────────── */

void touch_service_init(void)
{
    memset(s_ctx, 0, sizeof(s_ctx));

    if (!nvs_load_thresholds()) {
        ESP_LOGW(TAG, "threshold ausente no NVS — calibrando automaticamente");
        touch_service_calibrate();
    } else {
        ESP_LOGI(TAG, "threshold carregado do NVS:");
        for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
            ESP_LOGI(TAG, "  zona %d  threshold=%lu",
                     z, (unsigned long)s_threshold[z]);
        }
    }

    event_bus_subscribe(EVT_TOUCH_DETECTED, on_touch_detected);

    xTaskCreatePinnedToCore(touch_task, "TouchTask",
                            2048, NULL, 7, NULL, 1);

    ESP_LOGI(TAG,
             "TouchTask Core 1 pri 7  tick=%ums  debounce=%ums  zonas=%d",
             (unsigned)TASK_TICK_MS, (unsigned)DEBOUNCE_MS,
             HAL_TOUCH_ZONE_COUNT);
}
