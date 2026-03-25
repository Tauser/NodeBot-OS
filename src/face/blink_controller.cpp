#include "blink_controller.h"
#include "face_engine.h"
#include "face_params.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "BLINK";

/* ── Estado interno ─────────────────────────────────────────────────────── */
static volatile float s_energy   = 0.5f;
static volatile bool  s_suppress = false;

/* ── Duração dos keyframes ───────────────────────────────────────────────── */
static constexpr uint32_t KF1_MS    =  70u;
static constexpr uint32_t KF2_MS    =  30u;
static constexpr uint32_t KF3_MS    = 110u;
static constexpr uint32_t MARGIN_MS =  10u;

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static inline int8_t clamp_corner(int v)
{
    if (v >  40) return  40;
    if (v < -40) return -40;
    return (int8_t)v;
}

static inline int8_t clamp_y(int v)
{
    if (v >  60) return  60;
    if (v < -60) return -60;
    return (int8_t)v;
}

/* ── do_blink ────────────────────────────────────────────────────────────── */
static void do_blink(void)
{
    /* Salva expressão-alvo atual como base para os deltas */
    face_params_t base;
    face_engine_get_target(&base);

    /* ── KF1: ESPREMENDO — deltas sobre base, ease-in 70 ms ─────────────── */
    face_params_t kf1 = base;

    /* Cantos inferiores: +28 sobre o valor atual, clamped ±40 */
    kf1.bl_l = clamp_corner((int)base.bl_l + 28);
    kf1.br_l = clamp_corner((int)base.br_l + 28);
    kf1.bl_r = clamp_corner((int)base.bl_r + 28);
    kf1.br_r = clamp_corner((int)base.br_r + 28);

    /* Cantos superiores: +6 leve pressão do topo, clamped ±40 */
    kf1.tl_l = clamp_corner((int)base.tl_l + 6);
    kf1.tr_l = clamp_corner((int)base.tr_l + 6);
    kf1.tl_r = clamp_corner((int)base.tl_r + 6);
    kf1.tr_r = clamp_corner((int)base.tr_r + 6);

    /* Abertura: 8% da abertura atual */
    kf1.open_l = base.open_l * 0.08f;
    kf1.open_r = base.open_r * 0.08f;

    /* Y: sobe levemente (+delta negativo) */
    kf1.y_l = clamp_y((int)base.y_l - 4);
    kf1.y_r = clamp_y((int)base.y_r - 4);

    kf1.transition_ms = (uint16_t)KF1_MS;
    face_engine_apply_params(&kf1);

    vTaskDelay(pdMS_TO_TICKS(KF1_MS + MARGIN_MS));

    /* Verifica supressão durante KF1 — reabre com expressão original */
    if (s_suppress) {
        ESP_LOGD(TAG, "blink cancelado no KF1 — reabrindo");
        face_params_t abort = base;
        abort.transition_ms = (uint16_t)KF3_MS;
        face_engine_apply_params(&abort);
        vTaskDelay(pdMS_TO_TICKS(KF3_MS + MARGIN_MS));
        return;
    }

    /* ── KF2: FECHADO — 2% da abertura, cantos e y de KF1, hold 30 ms ───── */
    face_params_t kf2 = kf1;
    kf2.open_l        = base.open_l * 0.02f;
    kf2.open_r        = base.open_r * 0.02f;
    kf2.transition_ms = 0;   /* instantâneo */
    face_engine_apply_params(&kf2);

    vTaskDelay(pdMS_TO_TICKS(KF2_MS));

    /* ── KF3: ABRINDO — restaura base, ease-out 110 ms ──────────────────── */
    face_params_t kf3 = base;
    kf3.transition_ms = (uint16_t)KF3_MS;
    face_engine_apply_params(&kf3);

    vTaskDelay(pdMS_TO_TICKS(KF3_MS + MARGIN_MS));

    ESP_LOGD(TAG, "blink completo");
}

/* ── BlinkTask automática ───────────────────────────────────────────────── */
static void blink_task(void *arg)
{
    (void)arg;

    for (;;) {
        const float   e        = s_energy;
        const int32_t base_ms  = 5000 - (int32_t)(e * 2500.0f);
        const int32_t jitter   = (int32_t)(esp_random() % 2001u) - 1000;
        int32_t       delay_ms = base_ms + jitter;
        if (delay_ms <  800) delay_ms =  800;
        if (delay_ms > 7000) delay_ms = 7000;

        vTaskDelay(pdMS_TO_TICKS((uint32_t)delay_ms));

        if (s_suppress) {
            ESP_LOGD(TAG, "blink suprimido");
            continue;
        }

        do_blink();
    }
}

/* ── BlinkOnce — task descartável para trigger externo ─────────────────── */
static void blink_once_task(void *arg)
{
    (void)arg;
    do_blink();
    vTaskDelete(nullptr);
}

/* ── API pública ────────────────────────────────────────────────────────── */

void blink_controller_init(void)
{
    xTaskCreatePinnedToCore(
        blink_task,
        "BlinkTask",
        2048,
        nullptr,
        5,
        nullptr,
        0
    );
    ESP_LOGI(TAG, "BlinkTask iniciada (Core 0, pri 5)");
}

void blink_controller_trigger(void)
{
    xTaskCreatePinnedToCore(
        blink_once_task,
        "BlinkOnce",
        2048,
        nullptr,
        5,
        nullptr,
        0
    );
}

void blink_set_energy(float energy)
{
    if (energy < 0.0f) energy = 0.0f;
    if (energy > 1.0f) energy = 1.0f;
    s_energy = energy;
}

void blink_suppress(bool suppress)
{
    s_suppress = suppress;
}
