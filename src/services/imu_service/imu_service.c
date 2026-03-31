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

static const char *TAG = "imu_svc";

/* ── Configuração ──────────────────────────────────────────────────────── */
#define POLL_MS              50u
#define TASK_STACK           3072u
#define TASK_PRIO            7u

/* FALL: magnitude < threshold por > confirm */
#define FALL_MG_THRESHOLD    200
#define FALL_CONFIRM_MS      100u

/* SHAKE: variância da magnitude numa janela de 500ms */
#define SHAKE_WINDOW         10u     /* amostras (10 × 50ms = 500ms)           */
#define SHAKE_VAR_THRESHOLD  500000L /* mg² — piso em repouso ~100k; shake ~500k+ */
#define SHAKE_COOLDOWN_MS    2000u

/* TILT */
#define TILT_THRESHOLD_DEG   40.0f
#define TILT_CONFIRM_MS      2000u
#define TILT_COOLDOWN_MS     3000u

/* UPRIGHT */
#define UPRIGHT_MAX_DEG      15.0f

/* ── Estado ────────────────────────────────────────────────────────────── */
static volatile float    s_tilt_deg;
static volatile bool     s_upright;

/* ── Helpers ───────────────────────────────────────────────────────────── */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── Task ──────────────────────────────────────────────────────────────── */
static void imu_service_task(void *arg)
{
    (void)arg;

    /* Buffers para detecção */
    int32_t  mag_buf[SHAKE_WINDOW];
    uint8_t  mag_idx    = 0;
    uint8_t  mag_count  = 0;

    uint32_t fall_start      = 0u;
    bool     fall_active     = false;
    bool     fall_fired      = false;

    uint32_t tilt_start      = 0u;
    bool     tilt_active     = false;
    uint32_t tilt_cooldown   = 0u;
    uint32_t shake_cooldown  = 0u;

    memset(mag_buf, 0, sizeof(mag_buf));

    while (1) {
        uint32_t t = now_ms();

        int16_t ax, ay, az;
        if (imu_get_accel_mg(&ax, &ay, &az) != ESP_OK) {
            mag_count = 0;   /* invalida janela — leitura ruim corrompe variância */
            vTaskDelay(pdMS_TO_TICKS(POLL_MS));
            continue;
        }

        /* Magnitude em mg */
        int64_t mag_sq    = (int64_t)ax*ax + (int64_t)ay*ay + (int64_t)az*az;
        int32_t magnitude = (int32_t)sqrtf((float)mag_sq);

        /* Ângulo de inclinação: atan2(√(ax²+ay²), |az|) */
        float xy       = sqrtf((float)ax*(float)ax + (float)ay*(float)ay);
        float tilt     = atan2f(xy, fabsf((float)az)) * (180.0f / (float)M_PI);
        s_tilt_deg     = tilt;
        s_upright      = (tilt < UPRIGHT_MAX_DEG);

        /* ── FALL ──────────────────────────────────────────────────────── */
        if (magnitude < FALL_MG_THRESHOLD) {
            if (!fall_active) {
                fall_active = true;
                fall_start  = t;
                fall_fired  = false;
            } else if (!fall_fired && (t - fall_start) >= FALL_CONFIRM_MS) {
                fall_fired = true;
                motion_safety_emergency_stop();
                event_bus_publish(EVT_IMU_FALL, NULL, 0u, EVENT_PRIO_SAFETY);
                ESP_LOGW(TAG, "FALL — magnitude=%"PRId32"mg", magnitude);
            }
        } else {
            fall_active = false;
            fall_fired  = false;
        }

        /* ── SHAKE ─────────────────────────────────────────────────────── */
        mag_buf[mag_idx] = magnitude;
        mag_idx = (mag_idx + 1u) % SHAKE_WINDOW;
        if (mag_count < SHAKE_WINDOW) mag_count++;

        if (shake_cooldown > 0u) {
            shake_cooldown -= (shake_cooldown >= POLL_MS) ? POLL_MS : shake_cooldown;
        }

        if (mag_count == SHAKE_WINDOW && shake_cooldown == 0u) {
            int64_t sum = 0;
            for (int i = 0; i < (int)SHAKE_WINDOW; i++) sum += mag_buf[i];
            int32_t mean = (int32_t)(sum / (int64_t)SHAKE_WINDOW);

            int64_t var_sum = 0;
            for (int i = 0; i < (int)SHAKE_WINDOW; i++) {
                int32_t d = mag_buf[i] - mean;
                var_sum += (int64_t)d * d;
            }
            int32_t variance = (int32_t)(var_sum / (int64_t)SHAKE_WINDOW);

            if (variance > SHAKE_VAR_THRESHOLD) {
                imu_motion_event_t ev = { .value = (float)variance };
                event_bus_publish(EVT_IMU_SHAKE, &ev, sizeof(ev), EVENT_PRIO_BEHAVIOR);
                shake_cooldown = SHAKE_COOLDOWN_MS;
                ESP_LOGI(TAG, "SHAKE — variance=%"PRId32"mg²", variance);
            }
        }

        /* ── TILT ──────────────────────────────────────────────────────── */
        if (tilt_cooldown > 0u) {
            tilt_cooldown -= (tilt_cooldown >= POLL_MS) ? POLL_MS : tilt_cooldown;
        }

        if (tilt > TILT_THRESHOLD_DEG) {
            if (!tilt_active) {
                tilt_active = true;
                tilt_start  = t;
            } else if (tilt_cooldown == 0u &&
                       (t - tilt_start) >= TILT_CONFIRM_MS) {
                imu_motion_event_t ev = { .value = tilt };
                event_bus_publish(EVT_IMU_TILT, &ev, sizeof(ev), EVENT_PRIO_BEHAVIOR);
                tilt_cooldown = TILT_COOLDOWN_MS;
                tilt_active   = false;
                ESP_LOGI(TAG, "TILT — angle=%.1f°", (double)tilt);
            }
        } else {
            tilt_active = false;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

/* ── API pública ───────────────────────────────────────────────────────── */
bool imu_service_is_upright(void)
{
    return s_upright;
}

float imu_service_get_tilt_deg(void)
{
    return s_tilt_deg;
}

esp_err_t imu_service_init(void)
{
    s_tilt_deg = 0.0f;
    s_upright  = true;

    esp_err_t ret = imu_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "imu_init falhou: %s", esp_err_to_name(ret));
        return ret;
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        imu_service_task, "imu_svc",
        TASK_STACK, NULL, TASK_PRIO, NULL, 1);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "falha ao criar task (sem memória)");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ok — P%u Core1 poll=%ums", TASK_PRIO, POLL_MS);
    return ESP_OK;
}
