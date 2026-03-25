#include "gaze_service.h"
#include "face_engine.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <math.h>

static const char *TAG = "GAZE";

/* ── Constantes ──────────────────────────────────────────────────────────── */
#define GAZE_CLAMP          0.80f
#define OVERSHOOT_FACTOR    0.12f
#define RETURN_MS           80u
#define TICK_MS             100u

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ── Estado interno ─────────────────────────────────────────────────────── */
typedef enum {
    STATE_IDLE,
    STATE_OVERSHOOT,
    STATE_RETURN,
} gaze_state_t;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static volatile float       s_cur_x, s_cur_y;   /* posição interpolada atual  */
static gaze_state_t         s_state;
static float                s_src_x, s_src_y;   /* origem da fase atual       */
static float                s_ovr_x, s_ovr_y;   /* ponto de overshoot         */
static float                s_dst_x, s_dst_y;   /* destino final              */
static uint32_t             s_phase_start_ms;   /* timestamp início da fase   */
static uint32_t             s_phase_dur_ms;     /* duração da fase (ms)       */
static uint32_t             s_idle_next_ms;     /* quando a idle dispara      */

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Ease-out quadrático: desacelera no final */
static inline float ease_out(float t)
{
    return 1.0f - (1.0f - t) * (1.0f - t);
}

/* Número aleatório uniforme em [0, 1) */
static float randf(void)
{
    return (float)(esp_random() >> 1) / (float)0x7FFFFFFFu;
}

/*
 * Box-Muller — gera uma amostra N(mean, stddev).
 * u1 protegido de log(0) com epsilon.
 */
static float rand_gaussian(float mean, float stddev)
{
    float u1 = randf();
    float u2 = randf();
    if (u1 < 1e-7f) u1 = 1e-7f;
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return mean + stddev * z;
}

/* Agenda o próximo disparo da deriva idle: rand(2000, 5000) ms */
static void schedule_idle(void)
{
    uint32_t delay_ms = 2000u + (esp_random() % 3001u);
    s_idle_next_ms = now_ms() + delay_ms;
}

/* ── _set_target_internal ─────────────────────────────────────────────────
 * Chamado sob spinlock. Configura a saccade a partir de (s_cur_x, s_cur_y).
 */
static void set_target_locked(float x, float y, uint16_t duration_ms)
{
    x = clampf(x, -GAZE_CLAMP, GAZE_CLAMP);
    y = clampf(y, -GAZE_CLAMP, GAZE_CLAMP);

    s_src_x = s_cur_x;
    s_src_y = s_cur_y;

    /* overshoot = target + (target − current) × OVERSHOOT_FACTOR */
    s_ovr_x = clampf(x + (x - s_cur_x) * OVERSHOOT_FACTOR, -GAZE_CLAMP, GAZE_CLAMP);
    s_ovr_y = clampf(y + (y - s_cur_y) * OVERSHOOT_FACTOR, -GAZE_CLAMP, GAZE_CLAMP);
    s_dst_x = x;
    s_dst_y = y;

    s_phase_start_ms = now_ms();
    s_phase_dur_ms   = (uint32_t)((float)duration_ms * 0.7f);
    if (s_phase_dur_ms < 50u) s_phase_dur_ms = 50u;

    s_state = STATE_OVERSHOOT;
}

/* ── gaze_service_tick ───────────────────────────────────────────────────── */
static void gaze_service_tick(void)
{
    const uint32_t now = now_ms();

    taskENTER_CRITICAL(&s_mux);

    switch (s_state) {

    case STATE_OVERSHOOT: {
        float t = (float)(now - s_phase_start_ms) / (float)s_phase_dur_ms;
        if (t >= 1.0f) {
            /* Transição para RETURN */
            s_cur_x = s_ovr_x;
            s_cur_y = s_ovr_y;
            s_src_x = s_ovr_x;
            s_src_y = s_ovr_y;
            s_phase_start_ms = now;
            s_phase_dur_ms   = RETURN_MS;
            s_state = STATE_RETURN;
        } else {
            float te = ease_out(t);
            s_cur_x = s_src_x + (s_ovr_x - s_src_x) * te;
            s_cur_y = s_src_y + (s_ovr_y - s_src_y) * te;
        }
        break;
    }

    case STATE_RETURN: {
        float t = (float)(now - s_phase_start_ms) / (float)s_phase_dur_ms;
        if (t >= 1.0f) {
            s_cur_x = s_dst_x;
            s_cur_y = s_dst_y;
            s_state = STATE_IDLE;
            schedule_idle();
            ESP_LOGD(TAG, "saccade ok  dst=(%.2f, %.2f)", s_dst_x, s_dst_y);
        } else {
            float te = ease_out(t);
            s_cur_x = s_src_x + (s_dst_x - s_src_x) * te;
            s_cur_y = s_src_y + (s_dst_y - s_src_y) * te;
        }
        break;
    }

    case STATE_IDLE:
        if (now >= s_idle_next_ms) {
            float nx = clampf(rand_gaussian(0.0f, 0.3f), -GAZE_CLAMP, GAZE_CLAMP);
            float ny = clampf(rand_gaussian(0.0f, 0.2f), -GAZE_CLAMP, GAZE_CLAMP);
            /* Duração da saccade idle: 150–350 ms */
            uint16_t dur = (uint16_t)(150u + (esp_random() % 201u));
            set_target_locked(nx, ny, dur);
            ESP_LOGD(TAG, "idle saccade → (%.2f, %.2f) dur=%u ms", nx, ny, dur);
        }
        break;
    }

    const float cx = s_cur_x;
    const float cy = s_cur_y;

    taskEXIT_CRITICAL(&s_mux);

    /* Aplica diretamente no face engine (sem passar pelo event bus) */
    face_engine_set_gaze(cx, cy);

    /* Publica também no event bus para demais consumidores */
    gaze_event_t ev = { cx, cy };
    event_bus_publish(EVT_GAZE_UPDATE, &ev, sizeof(ev), EVENT_PRIO_COSMETIC);
}

/* ── GazeTask ────────────────────────────────────────────────────────────── */
static void gaze_task(void *arg)
{
    (void)arg;
    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        gaze_service_tick();
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TICK_MS));
    }
}

/* ── API pública ─────────────────────────────────────────────────────────── */

void gaze_service_init(void)
{
    s_cur_x = 0.0f;
    s_cur_y = 0.0f;
    s_state = STATE_IDLE;
    schedule_idle();

    xTaskCreatePinnedToCore(
        gaze_task,
        "GazeTask",
        3072,
        NULL,
        10,       /* prioridade 10 — abaixo do FaceRenderTask (20) */
        NULL,
        1         /* Core 1 */
    );
    ESP_LOGI(TAG, "GazeTask iniciada (Core 1, pri 10, tick=%u ms)", TICK_MS);
}

void gaze_service_set_target(float x, float y, uint16_t duration_ms)
{
    taskENTER_CRITICAL(&s_mux);
    set_target_locked(x, y, duration_ms);
    taskEXIT_CRITICAL(&s_mux);
}

void gaze_service_get(float *x, float *y)
{
    taskENTER_CRITICAL(&s_mux);
    if (x) *x = s_cur_x;
    if (y) *y = s_cur_y;
    taskEXIT_CRITICAL(&s_mux);
}
