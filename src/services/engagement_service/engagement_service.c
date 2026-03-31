#include "engagement_service.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/atomic.h"
#include "esp_log.h"
#include "esp_timer.h"

#define TAG "engage"

#define ENGAGEMENT_IDLE_MS    (10ULL * 60ULL * 1000ULL)   /* 10 min */
#define ENGAGEMENT_REPEAT_MS  (30ULL * 1000ULL)           /* 30 s   */

static volatile uint64_t s_last_interaction_us;
static volatile uint64_t s_last_publish_us;

static uint64_t now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}

/* ── callbacks ────────────────────────────────────────────────────────── */

static void on_interaction(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    s_last_interaction_us = now_us();
}

static void on_voice(uint16_t type, void *payload)
{
    (void)type;
    if (!payload) return;
    const uint8_t *active = (const uint8_t *)payload;
    if (*active) {
        s_last_interaction_us = now_us();
    }
}

/* ── task ─────────────────────────────────────────────────────────────── */

static void engagement_task(void *arg)
{
    (void)arg;
    /* aguarda 10s no boot antes de começar a monitorar */
    vTaskDelay(pdMS_TO_TICKS(10000));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));   /* poll a cada 5 s */

        uint64_t now        = now_us();
        uint64_t idle_us    = now - s_last_interaction_us;
        uint64_t idle_ms    = idle_us / 1000ULL;

        if (idle_ms < ENGAGEMENT_IDLE_MS) continue;

        /* inativo há > 10 min — emitir a cada 30 s */
        uint64_t since_pub_ms = (now - s_last_publish_us) / 1000ULL;
        if (since_pub_ms >= ENGAGEMENT_REPEAT_MS) {
            event_bus_publish(EVT_LOW_ENGAGEMENT, NULL, 0, EVENT_PRIO_BEHAVIOR);
            s_last_publish_us = now;
            ESP_LOGD(TAG, "EVT_LOW_ENGAGEMENT (idle %.0f min)",
                     (double)(idle_ms / 60000ULL));
        }
    }
}

/* ── init ─────────────────────────────────────────────────────────────── */

esp_err_t engagement_service_init(void)
{
    s_last_interaction_us = now_us();
    s_last_publish_us     = 0;

    event_bus_subscribe(EVT_TOUCH_PRESS,    on_interaction);
    event_bus_subscribe(EVT_WAKE_WORD,      on_interaction);
    event_bus_subscribe(EVT_VOICE_ACTIVITY, on_voice);

    BaseType_t rc = xTaskCreatePinnedToCore(engagement_task, "EngageTask",
                                             2048, NULL, 7, NULL, 1);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate falhou");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "init ok");
    return ESP_OK;
}
