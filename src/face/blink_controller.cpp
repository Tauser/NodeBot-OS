#include "blink_controller.h"
#include "face_engine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "BLINK";

static constexpr uint32_t CLOSE_MS  = 80u;
static constexpr uint32_t HOLD_MS   = 30u;
static constexpr uint32_t OPEN_MS   = 120u;
static constexpr uint32_t MIN_MS    = 2500u;
static constexpr uint32_t MAX_MS    = 5000u;
static constexpr uint32_t BIAS_MS   = 600u;
static constexpr uint32_t MARGIN_MS = 10u;

static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_blink_task = nullptr;
static float        s_energy = 0.5f;
static bool         s_suppress = false;

static float read_energy(void)
{
    float energy;
    taskENTER_CRITICAL(&s_state_mux);
    energy = s_energy;
    taskEXIT_CRITICAL(&s_state_mux);
    return energy;
}

static bool is_suppressed(void)
{
    bool suppress;
    taskENTER_CRITICAL(&s_state_mux);
    suppress = s_suppress;
    taskEXIT_CRITICAL(&s_state_mux);
    return suppress;
}

static uint32_t next_blink_delay_ms(void)
{
    const float energy = read_energy();

    int32_t min_ms = (int32_t)MIN_MS + (int32_t)((1.0f - energy) * (float)BIAS_MS);
    int32_t max_ms = (int32_t)MAX_MS - (int32_t)(energy * (float)BIAS_MS);

    if (min_ms < (int32_t)MIN_MS) min_ms = (int32_t)MIN_MS;
    if (max_ms > (int32_t)MAX_MS) max_ms = (int32_t)MAX_MS;
    if (min_ms > max_ms) min_ms = max_ms;

    const uint32_t span = (uint32_t)(max_ms - min_ms + 1);
    return (uint32_t)min_ms + (esp_random() % span);
}

static bool animate_blink_phase(float from, float to, uint32_t duration_ms)
{
    static constexpr uint32_t STEP_MS = 20u;
    const uint32_t steps = duration_ms / STEP_MS;

    if (steps == 0u) {
        face_engine_set_blink(to);
        return !is_suppressed();
    }

    for (uint32_t i = 1; i <= steps; ++i) {
        if (is_suppressed()) {
            face_engine_set_blink(0.0f);
            return false;
        }
        const float t = (float)i / (float)steps;
        face_engine_set_blink(from + (to - from) * t);
        vTaskDelay(pdMS_TO_TICKS(STEP_MS));
    }

    return !is_suppressed();
}

static void do_blink(void)
{
    if (!animate_blink_phase(0.0f, 0.82f, CLOSE_MS)) {
        ESP_LOGD(TAG, "blink abortado por supressao");
        return;
    }

    face_engine_set_blink(1.0f);
    vTaskDelay(pdMS_TO_TICKS(HOLD_MS));
    if (is_suppressed()) {
        face_engine_set_blink(0.0f);
        ESP_LOGD(TAG, "blink abortado durante hold");
        return;
    }

    animate_blink_phase(1.0f, 0.0f, OPEN_MS);
    face_engine_set_blink(0.0f);
    ESP_LOGD(TAG, "blink completo");
}

static void blink_task(void *arg)
{
    (void)arg;

    for (;;) {
        const uint32_t wait_ms = next_blink_delay_ms();
        const uint32_t pending = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));

        if (is_suppressed()) {
            continue;
        }

        if (pending > 1u) {
            ESP_LOGD(TAG, "coalescendo %lu triggers", (unsigned long)pending);
        }

        do_blink();
    }
}

void blink_controller_init(void)
{
    if (s_blink_task) {
        ESP_LOGW(TAG, "BlinkTask ja iniciada");
        return;
    }

    xTaskCreatePinnedToCore(
        blink_task,
        "BlinkTask",
        3072,
        nullptr,
        5,
        &s_blink_task,
        0
    );

    if (s_blink_task) {
        ESP_LOGI(TAG, "BlinkTask iniciada (Core 0, pri 5)");
    } else {
        ESP_LOGE(TAG, "falha ao criar BlinkTask");
    }
}

void blink_controller_trigger(void)
{
    if (!s_blink_task) {
        ESP_LOGW(TAG, "trigger ignorado: BlinkTask nao iniciada");
        return;
    }

    xTaskNotifyGive(s_blink_task);
}

void blink_set_energy(float energy)
{
    if (energy < 0.0f) energy = 0.0f;
    if (energy > 1.0f) energy = 1.0f;

    taskENTER_CRITICAL(&s_state_mux);
    s_energy = energy;
    taskEXIT_CRITICAL(&s_state_mux);
}

void blink_suppress(bool suppress)
{
    taskENTER_CRITICAL(&s_state_mux);
    s_suppress = suppress;
    taskEXIT_CRITICAL(&s_state_mux);

    if (!suppress) {
        face_engine_set_blink(0.0f);
        return;
    }

    face_engine_set_blink(0.0f);

    if (suppress && s_blink_task) {
        xTaskNotifyGive(s_blink_task);
    }
}
