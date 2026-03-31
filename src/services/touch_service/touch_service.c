#include "touch_service.h"
#include "touch_driver.h"
#include "event_bus.h"
#include "emotion_mapper.h"
#include "state_vector.h"
#include "config_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hal_init.h"

#include <string.h>
#include <inttypes.h>

static const char *TAG = "touch_svc";

/* ── Configuração ──────────────────────────────────────────────────────── */
#define POLL_MS              20u    /* período da task de varredura            */
#define DEBOUNCE_MS          50u    /* tempo mínimo de detecção contínua       */
#define CAL_SAMPLES         100u    /* amostras por zona na calibração         */
#define TASK_STACK          3072u
#define TASK_PRIO              7u

/* Limiares de gesto (duração do contato) */
#define TAP_MAX_MS           300u   /* release <= TAP_MAX  → tap rápido        */
#define PET_MIN_MS           800u   /* release >= PET_MIN  → carinho           */
#define HOLD_LONG_MS        3000u   /* release >= HOLD_LONG → hold longo       */

/* Double tap */
#define DOUBLE_TAP_WINDOW    400u   /* janela entre releases para double tap   */

/* Toque brusco */
#define INTENSITY_ROUGH      0.8f   /* intensidade mínima para "toque brusco"  */

/* Irritação */
#define IRRITATION_PER_TAP   1.0f   /* pontos por tap                          */
#define IRRITATION_ROUGH     0.5f   /* extra por toque brusco                  */
#define IRRITATION_PET      -2.0f   /* alívio por carinho                      */
#define IRRITATION_DECAY     0.01f  /* por tick de 20 ms (≈ 0.5/s)             */
#define IRRITATION_FORGET_MS 30000u /* zera após N ms sem toque                */
#define IRRITATION_WARN      3.0f   /* threshold → ANGRY leve                  */
#define IRRITATION_ANGRY     6.0f   /* threshold → ANGRY forte                 */
#define IRRITATION_MAX      12.0f

/* Arousal */
#define AROUSAL_BOOST        0.3f

/* NVS */
#define CAL_KEY_FMT         "tch_cal%d"

/* ── Estado por zona ───────────────────────────────────────────────────── */
typedef struct {
    /* debounce */
    bool     active;
    uint32_t first_ms;
    uint32_t press_start_ms;
    float    press_intensity;  /* intensidade registrada no press             */

    /* double tap */
    bool     tap_pending;      /* true = aguardando possível 2º tap           */
    uint32_t tap_release_ms;   /* quando o 1º tap foi solto                   */
} zone_state_t;

static uint32_t     s_cal[HAL_TOUCH_ZONE_COUNT];
static zone_state_t s_zones[HAL_TOUCH_ZONE_COUNT];

/* ── Irritação global ──────────────────────────────────────────────────── */
static float    s_irritation;
static uint32_t s_last_touch_ms;

/* ── Helpers ───────────────────────────────────────────────────────────── */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void irritation_add(float delta)
{
    s_irritation += delta;
    if (s_irritation < 0.0f)           s_irritation = 0.0f;
    if (s_irritation > IRRITATION_MAX) s_irritation = IRRITATION_MAX;
}

static void emit_angry(void)
{
    uint16_t trans = (s_irritation >= IRRITATION_ANGRY) ? 150u : 400u;
    emotion_mapper_apply(EMOTION_ANGRY, trans);
    ESP_LOGW(TAG, "irritation=%.1f → ANGRY (trans=%ums)",
             (double)s_irritation, (unsigned)trans);
}

/* ── NVS ───────────────────────────────────────────────────────────────── */
static void cal_save(void)
{
    char key[16];
    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        snprintf(key, sizeof(key), CAL_KEY_FMT, z);
        config_set_int(key, (int32_t)s_cal[z]);
    }
    ESP_LOGI(TAG, "thresholds salvos no NVS");
}

static bool cal_load(void)
{
    char key[16];
    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        snprintf(key, sizeof(key), CAL_KEY_FMT, z);
        int32_t v = config_get_int(key, 0);
        if (v <= 0) return false;
        s_cal[z] = (uint32_t)v;
    }
    return true;
}

/* ── Calibração ────────────────────────────────────────────────────────── */
void touch_service_calibrate(void)
{
    ESP_LOGI(TAG, "calibrando %d zonas — mantenha sem toque...", HAL_TOUCH_ZONE_COUNT);

    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        uint64_t sum = 0;
        for (int i = 0; i < CAL_SAMPLES; i++) {
            sum += touch_driver_read_raw((touch_zone_t)z);
            vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        }
        uint32_t mean = (uint32_t)(sum / CAL_SAMPLES);
        s_cal[z] = mean * 150u / 100u;
        ESP_LOGI(TAG, "  zona %d  media=%"PRIu32"  threshold=%"PRIu32,
                 z, mean, s_cal[z]);
    }

    cal_save();
    ESP_LOGI(TAG, "calibração concluída");
}

/* ── Reações ───────────────────────────────────────────────────────────── */

/* Chamado quando press é confirmado (debounced). */
static void on_press(touch_zone_t zone, uint32_t raw)
{
    float intensity = 0.0f;
    if (s_cal[zone] > 0u) {
        intensity = (float)(raw - s_cal[zone]) / (float)s_cal[zone];
        if (intensity < 0.0f) intensity = 0.0f;
    }

    s_zones[zone].press_intensity = intensity;
    s_last_touch_ms = now_ms();

    touch_event_t ev = {
        .zone_id     = (uint8_t)zone,
        .intensity   = intensity,
        .duration_ms = 0u,
    };
    event_bus_publish(EVT_TOUCH_PRESS, &ev, sizeof(ev), EVENT_PRIO_BEHAVIOR);

    state_vector_on_interaction();
    state_vector_on_touch(intensity >= INTENSITY_ROUGH);
    g_state.being_touched = true;

    float new_arousal = g_state.mood_arousal + AROUSAL_BOOST;
    if (new_arousal > 1.0f) new_arousal = 1.0f;
    g_state.mood_arousal = new_arousal;

    ESP_LOGD(TAG, "press zona=%d intensity=%.2f", (int)zone, (double)intensity);
}

/* Chamado no release; classifica o gesto pela duração. */
static void on_release(touch_zone_t zone, uint32_t duration_ms)
{
    touch_event_t ev = {
        .zone_id     = (uint8_t)zone,
        .intensity   = 0.0f,
        .duration_ms = duration_ms,
    };
    event_bus_publish(EVT_TOUCH_RELEASE, &ev, sizeof(ev), EVENT_PRIO_BEHAVIOR);

    g_state.being_touched = false;

    if (duration_ms >= HOLD_LONG_MS) {
        /* Hold longo: cansado/sonolento */
        ESP_LOGI(TAG, "hold_long zona=%d dur=%"PRIu32"ms", (int)zone, duration_ms);
        emotion_mapper_apply(EMOTION_SAD, 600u);

    } else if (duration_ms >= PET_MIN_MS) {
        /* Carinho: acalma o robô */
        irritation_add(IRRITATION_PET);
        state_vector_on_pet();
        emotion_mapper_apply(EMOTION_HAPPY, 300u);
        ESP_LOGI(TAG, "pet zona=%d dur=%"PRIu32"ms irr=%.1f",
                 (int)zone, duration_ms, (double)s_irritation);

    } else if (duration_ms <= TAP_MAX_MS) {
        /* Tap rápido: acumula irritação */
        bool rough = (s_zones[zone].press_intensity >= INTENSITY_ROUGH);
        irritation_add(IRRITATION_PER_TAP + (rough ? IRRITATION_ROUGH : 0.0f));

        if (s_irritation >= IRRITATION_WARN) {
            /* Robô irritado: sobrescreve gesto */
            emit_angry();
            s_zones[zone].tap_pending = false;
        } else if (s_zones[zone].tap_pending) {
            /* 2º tap dentro da janela: double tap */
            ESP_LOGI(TAG, "double_tap zona=%d irr=%.1f", (int)zone, (double)s_irritation);
            emotion_mapper_apply(EMOTION_FOCUSED, 200u);
            s_zones[zone].tap_pending = false;
        } else {
            /* 1º tap: aguarda possível 2º dentro da janela */
            s_zones[zone].tap_pending    = true;
            s_zones[zone].tap_release_ms = now_ms();
        }

        ESP_LOGD(TAG, "tap zona=%d dur=%"PRIu32"ms rough=%d irr=%.1f",
                 (int)zone, duration_ms, (int)rough, (double)s_irritation);
    }
    /* duração entre TAP_MAX+1 e PET_MIN-1: contato acidental, ignora */
}

/* ── Task de varredura ─────────────────────────────────────────────────── */
static void touch_service_task(void *arg)
{
    (void)arg;

    while (1) {
        uint32_t t = now_ms();

        /* Decaimento passivo de irritação */
        if (s_irritation > 0.0f) {
            s_irritation -= IRRITATION_DECAY;
            if (s_irritation < 0.0f) s_irritation = 0.0f;
        }

        /* Esquecimento total após pausa longa sem toque */
        if (s_irritation > 0.0f &&
            s_last_touch_ms > 0u &&
            (t - s_last_touch_ms) >= IRRITATION_FORGET_MS) {
            ESP_LOGD(TAG, "irritação zerada por inatividade");
            s_irritation = 0.0f;
        }

        for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
            uint32_t      raw     = touch_driver_read_raw((touch_zone_t)z);
            bool          touched = (s_cal[z] > 0u) && (raw > s_cal[z]);
            zone_state_t *zs      = &s_zones[z];

            if (touched) {
                if (!zs->active) {
                    if (zs->first_ms == 0u) {
                        zs->first_ms = t;
                    } else if ((t - zs->first_ms) >= DEBOUNCE_MS) {
                        zs->active         = true;
                        zs->press_start_ms = zs->first_ms;
                        on_press((touch_zone_t)z, raw);
                    }
                }
            } else {
                if (zs->active) {
                    uint32_t dur = t - zs->press_start_ms;
                    on_release((touch_zone_t)z, dur);
                }
                zs->active         = false;
                zs->first_ms       = 0u;
                zs->press_start_ms = 0u;
            }

            /* Timeout do double tap: confirma tap simples se janela expirou */
            if (zs->tap_pending && !zs->active &&
                (t - zs->tap_release_ms) >= DOUBLE_TAP_WINDOW) {
                zs->tap_pending = false;
                if (s_irritation < IRRITATION_WARN) {
                    emotion_mapper_apply(EMOTION_SURPRISED, 200u);
                    ESP_LOGD(TAG, "tap zona=%d confirmado (single)", z);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

/* ── Init ──────────────────────────────────────────────────────────────── */
esp_err_t touch_service_init(void)
{
    touch_driver_init();
    memset(s_zones, 0, sizeof(s_zones));
    memset(s_cal,   0, sizeof(s_cal));
    s_irritation    = 0.0f;
    s_last_touch_ms = 0u;

    if (cal_load()) {
        ESP_LOGI(TAG, "thresholds carregados do NVS:");
        for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++)
            ESP_LOGI(TAG, "  zona %d = %"PRIu32, z, s_cal[z]);
    } else {
        ESP_LOGI(TAG, "NVS sem calibração — calibração automática");
        touch_service_calibrate();
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        touch_service_task, "touch_svc",
        TASK_STACK, NULL, TASK_PRIO, NULL, 1);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "falha ao criar task (sem memória)");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ok — P%u Core1 poll=%ums debounce=%ums", TASK_PRIO, POLL_MS, DEBOUNCE_MS);
    return ESP_OK;
}
