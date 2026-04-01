#include "face_tracker.h"
#include "presence_detector.h"
#include "gesture_service.h"
#include "motion_safety_service.h"
#include "dialogue_state_service.h"
#include "event_bus.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "face_tracker";

/* ── Parâmetros (ajustar na bancada) ─────────────────────────────────────── */
#define TICK_MS           50u      /* 20 Hz                                  */
#define DEAD_ZONE         0.05f    /* erro < 5% do frame → sem movimento     */
#define KP                20.0f   /* ganho proporcional (graus por erro)     */
#define KD                 5.0f   /* ganho derivativo                        */
#define MAX_STEP_DEG      10.0f   /* limite de velocidade por tick           */
#define PAN_MIN_DEG      -75.0f   /* limite físico esquerdo                  */
#define PAN_MAX_DEG       75.0f   /* limite físico direito                   */
#define ABSENT_TICKS_MAX 100u     /* 5 s a 20 Hz → retornar ao centro       */
#define COOLDOWN_MS       500u    /* pausa após fim de diálogo               */

/* ── Estado ───────────────────────────────────────────────────────────────── */
static volatile bool s_enabled       = true;
static volatile bool s_cooldown      = false;
static uint32_t      s_cooldown_end  = 0u;    /* timestamp ms */
static float         s_last_error    = 0.0f;
static float         s_current_deg   = 0.0f;  /* posição acumulada estimada */
static uint32_t      s_absent_ticks  = 0u;
static bool          s_was_tracking  = false;

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Cooldown ─────────────────────────────────────────────────────────────── */

static void on_dialogue_state(uint16_t type, void *payload)
{
    (void)type;
    if (!payload) return;
    const dialogue_state_event_t *ev = (const dialogue_state_event_t *)payload;

    if (ev->state != DIALOGUE_IDLE) {
        /* Robô falando / processando → ativar cooldown */
        s_cooldown     = true;
        s_cooldown_end = now_ms() + COOLDOWN_MS;
    } else {
        /* Voltou a IDLE: mantém cooldown por COOLDOWN_MS a partir de agora */
        s_cooldown_end = now_ms() + COOLDOWN_MS;
    }
}

/* ── Lógica de tracking por tick ─────────────────────────────────────────── */

static void tracker_tick(uint32_t t_ms)
{
    /* Verifica expiração do cooldown */
    if (s_cooldown && t_ms >= s_cooldown_end) {
        s_cooldown = false;
    }

    face_detection_result_t face = presence_detector_get_last();

    /* Sem detecção ou cooldown ativo: contabiliza ausência */
    if (!face.detected || s_cooldown || !s_enabled) {
        s_absent_ticks++;
        if (s_absent_ticks >= ABSENT_TICKS_MAX) {
            s_absent_ticks = 0u;
            s_last_error   = 0.0f;

            if (s_current_deg != 0.0f) {
                /* Retorna ao centro usando GESTURE_REST */
                gesture_perform(GESTURE_REST);
                s_current_deg = 0.0f;
                ESP_LOGD(TAG, "retorno ao centro (ausência 5s)");

                face_tracker_event_t ev = {
                    .target_deg      = 0.0f,
                    .current_deg     = 0.0f,
                    .tracking_active = false,
                };
                event_bus_publish(EVT_FACE_TRACKER_UPDATE, &ev, sizeof(ev),
                                  EVENT_PRIO_COSMETIC);
            }
        }
        s_was_tracking = false;
        return;
    }

    s_absent_ticks = 0u;

    /* Erro: positivo = rosto à direita do centro */
    float error_x = face.x - 0.5f;

    /* Dead zone */
    if (error_x > -DEAD_ZONE && error_x < DEAD_ZONE) {
        s_last_error = 0.0f;
        return;
    }

    /* PID (P + D) */
    float delta = KP * error_x + KD * (error_x - s_last_error);
    s_last_error = error_x;

    /* Limita velocidade */
    if (delta >  MAX_STEP_DEG) delta =  MAX_STEP_DEG;
    if (delta < -MAX_STEP_DEG) delta = -MAX_STEP_DEG;

    float target_deg = clampf(s_current_deg + delta, PAN_MIN_DEG, PAN_MAX_DEG);

    /* Verifica segurança antes de mover */
    if (!motion_safety_is_safe()) {
        ESP_LOGW(TAG, "motion_safety BLOCKED — tracking suspenso");
        s_was_tracking = false;
        return;
    }

    /* Submete gesto de pan (delta incremental) */
    float actual_delta = target_deg - s_current_deg;
    if (actual_delta == 0.0f) return;

    gesture_command_t cmd = {
        .target        = GESTURE_TARGET_HEAD,
        .pan_delta_deg = actual_delta,
        .tilt_delta_deg = 0.0f,
        .duration_ms   = TICK_MS,
        .hold_ms       = 0u,
        .priority      = GESTURE_PRIORITY_LOW,
        .interruptible = true,
    };

    if (gesture_service_submit(&cmd, t_ms)) {
        s_current_deg = target_deg;
    }

    /* Publica update apenas quando muda de estado de tracking */
    if (!s_was_tracking) {
        face_tracker_event_t ev = {
            .target_deg      = target_deg,
            .current_deg     = s_current_deg,
            .tracking_active = true,
        };
        event_bus_publish(EVT_FACE_TRACKER_UPDATE, &ev, sizeof(ev),
                          EVENT_PRIO_COSMETIC);
        s_was_tracking = true;
    }
}

/* ── Task ─────────────────────────────────────────────────────────────────── */

static void tracker_task(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TICK_MS));
        tracker_tick(now_ms());
    }
}

/* ── API pública ──────────────────────────────────────────────────────────── */

void face_tracker_set_enabled(bool enabled)
{
    s_enabled = enabled;
}

esp_err_t face_tracker_init(void)
{
    event_bus_subscribe(EVT_DIALOGUE_STATE_CHANGED, on_dialogue_state);

    /* Core 1, P9 — abaixo de Behavior (P12) e acima de Storage (P5) */
    BaseType_t ret = xTaskCreatePinnedToCore(
        tracker_task, "face_tracker", 3072, NULL, 9, NULL, 1
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "falha ao criar tracker_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ok — Kp=%.0f Kd=%.0f dead_zone=%.0f%% max_step=%.0f deg",
             (double)KP, (double)KD, (double)(DEAD_ZONE * 100.0f),
             (double)MAX_STEP_DEG);
    return ESP_OK;
}
