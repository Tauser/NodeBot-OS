#include "imu_service.h"
#include "imu_driver.h"
#include "event_bus.h"
#include "motion_safety_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "IMU_SVC";

/* ── Configuração ─────────────────────────────────────────────────── */
#define TASK_TICK_MS        50u

/* FALL */
#define FALL_THRESH_MG2     40000L     /* 200² mg²                           */
#define FALL_TICKS          2u         /* 2 × 50 ms = 100 ms                 */

/* SHAKE — janela deslizante de 10 amostras (500 ms / 50 ms) */
#define SHAKE_WIN           10u
#define SHAKE_THRESH_VAR    200.0f     /* mg² — variância total dos 3 eixos  */
#define SHAKE_COOLDOWN      20u        /* 20 × 50 ms = 1 s entre eventos     */

/* TILT */
#define TILT_THRESH_DEG     40.0f
#define TILT_TICKS          40u        /* 40 × 50 ms = 2 s                   */

/* ── Estado — somente escrito pela IMUTask ────────────────────────── */

/* FALL */
static uint8_t  s_fall_ticks = 0;
static bool     s_fall_fired = false;

/* SHAKE */
static int16_t  s_win_x[SHAKE_WIN];
static int16_t  s_win_y[SHAKE_WIN];
static int16_t  s_win_z[SHAKE_WIN];
static uint8_t  s_win_head   = 0;
static uint8_t  s_win_count  = 0;      /* amostras válidas (0..SHAKE_WIN)   */
static uint8_t  s_shake_cool = 0;

/* TILT */
static uint16_t s_tilt_ticks = 0;
static bool     s_tilt_fired = false;

/* API pública — leitura atômica por outras tasks */
static volatile float s_tilt_deg = 0.0f;
static volatile bool  s_upright  = true;

/* ── Helpers ──────────────────────────────────────────────────────── */

static inline float variance_f(const int16_t *buf, uint8_t n)
{
    if (n < 2u) return 0.0f;
    float sum = 0.0f, sum_sq = 0.0f;
    for (uint8_t i = 0; i < n; i++) {
        float v = (float)buf[i];
        sum    += v;
        sum_sq += v * v;
    }
    float mean = sum / (float)n;
    return sum_sq / (float)n - mean * mean;
}

static void publish_motion(motion_type_t type, float value)
{
    motion_event_t evt = { .type = type, .value = value };
    event_bus_publish(EVT_MOTION_DETECTED, &evt, sizeof(evt), EVENT_PRIO_SAFETY);
}

/* ── IMUTask ──────────────────────────────────────────────────────── */

static void imu_task(void *arg)
{
    (void)arg;
    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TASK_TICK_MS));

        int16_t ax, ay, az;
        if (imu_get_accel_mg(&ax, &ay, &az) != ESP_OK) continue;

        /* ── Tilt (ângulo da vertical) ──────────────────────────── */
        {
            const float ax_f  = (float)ax;
            const float ay_f  = (float)ay;
            const float az_f  = (float)az;
            const float mag   = sqrtf(ax_f*ax_f + ay_f*ay_f + az_f*az_f);

            float tilt = 0.0f;
            if (mag > 100.0f) {   /* ignora leituras espúrias com mag muito baixa */
                tilt = atan2f(sqrtf(ax_f*ax_f + ay_f*ay_f), az_f)
                       * (180.0f / (float)M_PI);
            }
            s_tilt_deg = tilt;
            s_upright  = (tilt < TILT_THRESH_DEG);
        }

        /* ── (1) FALL: |accel|² < 200² mg² por > 100 ms ─────────── */
        {
            const int32_t mag2 = (int32_t)ax*(int32_t)ax
                               + (int32_t)ay*(int32_t)ay
                               + (int32_t)az*(int32_t)az;

            if (!s_fall_fired && mag2 < FALL_THRESH_MG2) {
                if (++s_fall_ticks >= FALL_TICKS) {
                    s_fall_fired = true;
                    ESP_LOGE(TAG, "FREEFALL detectado — acionando emergency stop");
                    motion_safety_emergency_stop();
                    publish_motion(MOTION_FALL, 0.0f);
                }
            } else if (mag2 >= FALL_THRESH_MG2) {
                s_fall_ticks = 0;
                /* s_fall_fired não é re-armado — requer reset do sistema */
            }
        }

        /* ── (2) SHAKE: variância janela 500 ms > 200 mg² ───────── */
        {
            /* Insere amostra no ring buffer */
            s_win_x[s_win_head] = ax;
            s_win_y[s_win_head] = ay;
            s_win_z[s_win_head] = az;
            s_win_head = (uint8_t)((s_win_head + 1u) % SHAKE_WIN);
            if (s_win_count < SHAKE_WIN) s_win_count++;

            if (s_shake_cool > 0u) {
                s_shake_cool--;
            } else if (s_win_count == SHAKE_WIN) {
                const float var_total = variance_f(s_win_x, SHAKE_WIN)
                                      + variance_f(s_win_y, SHAKE_WIN)
                                      + variance_f(s_win_z, SHAKE_WIN);

                if (var_total > SHAKE_THRESH_VAR) {
                    const float intensity = sqrtf(var_total);
                    ESP_LOGI(TAG, "SHAKE  var=%.1f mg²  intensity=%.1f mg",
                             var_total, intensity);
                    publish_motion(MOTION_SHAKE, intensity);
                    s_shake_cool = SHAKE_COOLDOWN;
                }
            }
        }

        /* ── (3) TILT: ângulo > 40° por > 2 s ───────────────────── */
        {
            if (s_tilt_deg > TILT_THRESH_DEG) {
                if (!s_tilt_fired && ++s_tilt_ticks >= TILT_TICKS) {
                    s_tilt_fired = true;
                    ESP_LOGW(TAG, "TILT  ângulo=%.1f° — evento publicado",
                             s_tilt_deg);
                    publish_motion(MOTION_TILT, s_tilt_deg);
                }
            } else {
                s_tilt_ticks = 0;
                s_tilt_fired = false;   /* re-arma quando volta a <40° */
            }
        }
    }
}

/* ── Init ─────────────────────────────────────────────────────────── */

void imu_service_init(void)
{
    memset(s_win_x, 0, sizeof(s_win_x));
    memset(s_win_y, 0, sizeof(s_win_y));
    memset(s_win_z, 0, sizeof(s_win_z));

    xTaskCreatePinnedToCore(imu_task, "IMUTask",
                            3072, NULL, 7, NULL, 1);

    ESP_LOGI(TAG,
             "IMUTask Core 1 pri 7  tick=%ums  "
             "fall<200mg/>100ms  shake_var>200mg²  tilt>40°/>2s",
             (unsigned)TASK_TICK_MS);
}

/* ── API pública ──────────────────────────────────────────────────── */

bool imu_service_is_upright(void)   { return s_upright;  }
float imu_service_get_tilt_deg(void) { return s_tilt_deg; }
