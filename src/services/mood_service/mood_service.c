#include "mood_service.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define TAG "mood"

/*
 * Decay por tick de 1 s: factor = 1 - (1 / tau_s)
 *   valence:  tau = 4 h = 14400 s  →  factor = 0.999931
 *   arousal:  tau = 30 min = 1800 s →  factor = 0.999444
 *
 * Aproximação linear válida quando tick << tau.
 */
#define DECAY_FACTOR_VALENCE  0.999931f
#define DECAY_FACTOR_AROUSAL  0.999444f

#define MOOD_PUBLISH_THRESHOLD  0.05f

static SemaphoreHandle_t s_mutex;
static float s_valence;
static float s_arousal;
static float s_pub_valence;
static float s_pub_arousal;

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float fabsf_local(float v) { return v < 0.0f ? -v : v; }

/* Chama com mutex já adquirido. event_bus_publish copia o payload — seguro. */
static void publish_if_changed(void)
{
    if (fabsf_local(s_valence - s_pub_valence) >= MOOD_PUBLISH_THRESHOLD ||
        fabsf_local(s_arousal - s_pub_arousal) >= MOOD_PUBLISH_THRESHOLD)
    {
        mood_event_t ev = { .valence = s_valence, .arousal = s_arousal };
        s_pub_valence = s_valence;
        s_pub_arousal = s_arousal;
        event_bus_publish(EVT_MOOD_CHANGED, &ev, sizeof(ev), EVENT_PRIO_BEHAVIOR);
    }
}

/* ── task ─────────────────────────────────────────────────────────────── */

static void mood_task(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_valence *= DECAY_FACTOR_VALENCE;
        s_arousal *= DECAY_FACTOR_AROUSAL;
        publish_if_changed();
        xSemaphoreGive(s_mutex);
    }
}

/* ── callbacks EventBus ───────────────────────────────────────────────── */

static void on_touch(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    /* toque aumenta valence e arousal levemente */
    mood_service_boost(0.05f, 0.10f);
}

static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    mood_service_boost(0.03f, 0.15f);
}

static void on_tts_done(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    /* após falar, leve queda de arousal */
    mood_service_boost(0.02f, -0.05f);
}

/* ── API ──────────────────────────────────────────────────────────────── */

esp_err_t mood_service_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    s_valence     = 0.0f;
    s_arousal     = 0.2f;   /* leve arousal inicial ao acordar */
    s_pub_valence = s_valence;
    s_pub_arousal = s_arousal;

    event_bus_subscribe(EVT_TOUCH_PRESS, on_touch);
    event_bus_subscribe(EVT_WAKE_WORD,   on_wake_word);
    event_bus_subscribe(EVT_TTS_DONE,    on_tts_done);

    BaseType_t rc = xTaskCreatePinnedToCore(mood_task, "MoodTask",
                                             2048, NULL, 9, NULL, 1);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate falhou");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "init ok");
    return ESP_OK;
}

void mood_service_boost(float d_valence, float d_arousal)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    s_valence = clampf(s_valence + d_valence, -1.0f,  1.0f);
    s_arousal = clampf(s_arousal + d_arousal,  0.0f,  1.0f);
    publish_if_changed();
    xSemaphoreGive(s_mutex);
}

void mood_service_get(float *valence, float *arousal)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
    if (valence) *valence = s_valence;
    if (arousal) *arousal = s_arousal;
    xSemaphoreGive(s_mutex);
}
