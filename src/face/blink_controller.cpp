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
static portMUX_TYPE    s_blink_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool   s_blink_in_progress = false;

/* ── Duração dos keyframes ───────────────────────────────────────────────── */
static constexpr uint32_t KF1_MS    =  70u;
static constexpr uint32_t KF2_MS    =  30u;
static constexpr uint32_t KF3_MS    = 110u;
static constexpr uint32_t MARGIN_MS =  10u;
static constexpr float    KF1_OPEN_SCALE = 0.24f;
static constexpr float    KF2_OPEN_SCALE = 0.14f;
static constexpr float    BLINK_OPEN_FLOOR = 0.14f;
static face_params_t       s_last_open_base = FACE_NEUTRAL;

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

static inline bool eyes_already_small(const face_params_t &p)
{
    return (p.open_l <= 0.50f) || (p.open_r <= 0.50f);
}

static inline float blink_open_value(float base_open, float scale)
{
    float v = base_open * scale;
    return (v < BLINK_OPEN_FLOOR) ? BLINK_OPEN_FLOOR : v;
}

static void remember_open_base(const face_params_t &p)
{
    if (!eyes_already_small(p)) {
        s_last_open_base = p;
    }
}

static bool blink_try_begin(void)
{
    bool acquired = false;

    taskENTER_CRITICAL(&s_blink_lock);
    if (!s_blink_in_progress) {
        s_blink_in_progress = true;
        acquired = true;
    }
    taskEXIT_CRITICAL(&s_blink_lock);

    return acquired;
}

static void blink_end(void)
{
    taskENTER_CRITICAL(&s_blink_lock);
    s_blink_in_progress = false;
    taskEXIT_CRITICAL(&s_blink_lock);
}

/* ── do_blink ────────────────────────────────────────────────────────────── */
static void do_blink(void)
{
    if (!blink_try_begin()) {
        ESP_LOGD(TAG, "blink skip: already in progress");
        return;
    }

    /* Salva expressão renderizada atual como base visual e o target lógico atual
     * para restaurá-lo ao final do blink. Sem isso, o blink passa a virar o
     * `_dst` do FaceEngine e o recovery pode perseguir um target semi-fechado. */
    face_params_t base;
    face_params_t steady_target;
    face_engine_get_current(&base);
    face_engine_get_target(&steady_target);

    if (eyes_already_small(base)) {
        ESP_LOGD(TAG, "blink skip: eyes already small");
        face_engine_get_target(&base);
        if (eyes_already_small(base)) {
            base = s_last_open_base;
        }
    } else {
        remember_open_base(base);
    }

    if (eyes_already_small(base)) {
        blink_end();
        return;
    }

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

    /* Evita que o blink passe a perseguir um alvo "quase zero" por muitos
     * frames em runtime. No painel a leitura continua de piscada, mas sem
     * empurrar o target para 0.08/0.02 como antes. */
    kf1.open_l = blink_open_value(base.open_l, KF1_OPEN_SCALE);
    kf1.open_r = blink_open_value(base.open_r, KF1_OPEN_SCALE);

    /* Y: sobe levemente (+delta negativo) */
    kf1.y_l = clamp_y((int)base.y_l - 4);
    kf1.y_r = clamp_y((int)base.y_r - 4);

    kf1.transition_ms = (uint16_t)KF1_MS;
    face_engine_apply_params(&kf1);

    vTaskDelay(pdMS_TO_TICKS(KF1_MS + MARGIN_MS));

    /* Verifica supressão durante KF1 — reabre com expressão original */
    if (s_suppress) {
        ESP_LOGD(TAG, "blink cancelado no KF1 — reabrindo");
        face_params_t abort = steady_target;
        if (eyes_already_small(abort)) {
            abort = base;
        }
        abort.transition_ms = (uint16_t)KF3_MS;
        face_engine_apply_params(&abort);
        vTaskDelay(pdMS_TO_TICKS(KF3_MS + MARGIN_MS));
        blink_end();
        return;
    }

    /* ── KF2: FECHADO — menos agressivo para não "colar" small-eyes ─────── */
    face_params_t kf2 = kf1;
    kf2.open_l        = blink_open_value(base.open_l, KF2_OPEN_SCALE);
    kf2.open_r        = blink_open_value(base.open_r, KF2_OPEN_SCALE);
    kf2.transition_ms = 0;   /* instantâneo */
    face_engine_apply_params(&kf2);

    vTaskDelay(pdMS_TO_TICKS(KF2_MS));

    /* ── KF3: ABRINDO — restaura base, ease-out 110 ms ──────────────────── */
    face_params_t kf3 = steady_target;
    if (eyes_already_small(kf3)) {
        kf3 = base;
    }
    kf3.transition_ms = (uint16_t)KF3_MS;
    face_engine_apply_params(&kf3);

    vTaskDelay(pdMS_TO_TICKS(KF3_MS + MARGIN_MS));

    ESP_LOGD(TAG, "blink completo");
    blink_end();
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
    face_params_t base;
    face_engine_get_current(&base);
    remember_open_base(base);
    if (eyes_already_small(base)) {
        ESP_LOGD(TAG, "blink trigger skip: eyes already small");
        return;
    }

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
