#include "motion_safety_service.h"
#include "scs0009.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SAFETY";

/* ── Configuração ─────────────────────────────────────────────────── */
#define TASK_TICK_MS           10u   /* período da task (ms) — mín 1 tick (100 Hz = 10 ms) */
#define OVERCURRENT_MA         800   /* limiar de overcurrent (mA)        */
#define OVERCURRENT_THRESH_MS  80u   /* tempo acumulado para trip (ms)    */
#define HEARTBEAT_TIMEOUT_MS   500u  /* timeout do watchdog (ms)          */
#define SERVO_COUNT            2u    /* número de servos monitorados      */

/* ── Estado ───────────────────────────────────────────────────────── */
static volatile bool     s_safe       = true;
static volatile uint32_t s_last_hb_ms = 0u;
static volatile bool     s_hb_armed   = false; /* arma após primeiro heartbeat */

/* Acumuladores de overcurrent — exclusivos da MotionSafetyTask */
static uint32_t s_oc_acc[SERVO_COUNT];

/* ── Helpers ──────────────────────────────────────────────────────── */

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── API pública ──────────────────────────────────────────────────── */

void motion_safety_feed_heartbeat(void)
{
    s_last_hb_ms = now_ms();
    s_hb_armed   = true;
}

void motion_safety_emergency_stop(void)
{
    scs0009_set_torque_enable(false);
    s_safe = false;
    ESP_LOGE(TAG, "EMERGENCY STOP");
}

bool motion_safety_is_safe(void)
{
    return s_safe;
}

/* ── Callback do EventBus ─────────────────────────────────────────── */

static void on_heartbeat(uint16_t type, void *payload)
{
    (void)type;
    (void)payload;
    motion_safety_feed_heartbeat();
}

/* ── MotionSafetyTask ─────────────────────────────────────────────── */

static void safety_task(void *arg)
{
    (void)arg;

    /* Período de graça: arma o timestamp com now() para que o watchdog
     * não dispare antes de receber o primeiro heartbeat real. */
    s_last_hb_ms = now_ms();
    s_hb_armed   = false;

    for (size_t i = 0; i < SERVO_COUNT; i++) {
        s_oc_acc[i] = 0u;
    }

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TASK_TICK_MS));

        /* Sistema já bloqueado — aguarda reset externo */
        if (!s_safe) continue;

        const uint32_t now = now_ms();

        /* ── (1) Heartbeat watchdog ─────────────────────────────── */
        if (s_hb_armed && (now - s_last_hb_ms) > HEARTBEAT_TIMEOUT_MS) {
            ESP_LOGE(TAG, "heartbeat timeout (%lu ms) — parando servos",
                     (unsigned long)(now - s_last_hb_ms));
            scs0009_set_torque_enable(false);
            s_safe = false;
            /* Falha de watchdog não publica EVT_SERVO_BLOCKED —
             * é uma condição de sistema, não de hardware.          */
            continue;
        }

        /* ── (2) Overcurrent — um servo por tick para manter latência baixa */
        for (size_t i = 0; i < SERVO_COUNT; i++) {
            const int32_t ma = get_current_ma((uint8_t)i);

            if (ma > OVERCURRENT_MA) {
                s_oc_acc[i] += TASK_TICK_MS;

                if (s_oc_acc[i] >= OVERCURRENT_THRESH_MS) {
                    ESP_LOGE(TAG, "overcurrent servo %u: %ld mA (acum %lu ms)",
                             (unsigned)i, (long)ma, (unsigned long)s_oc_acc[i]);

                    scs0009_set_torque_enable(false);
                    s_safe = false;

                    servo_blocked_event_t evt = {
                        .servo_id   = (uint8_t)i,
                        .current_ma = (uint16_t)(ma > 65535 ? 65535 : ma),
                    };
                    event_bus_publish(EVT_SERVO_BLOCKED,
                                      &evt, sizeof(evt),
                                      EVENT_PRIO_SAFETY);
                    break;  /* sai do loop — servos já parados */
                }
            } else {
                s_oc_acc[i] = 0u;  /* reset ao voltar para corrente normal */
            }
        }
    }
}

/* ── Init ─────────────────────────────────────────────────────────── */

void motion_safety_init(void)
{
    event_bus_subscribe(EVT_SERVICE_HEARTBEAT, on_heartbeat);

    xTaskCreatePinnedToCore(safety_task, "MotionSafetyTask",
                            2048, NULL, 22, NULL, 1);

    ESP_LOGI(TAG,
             "MotionSafetyTask Core 1 pri 22  tick=%ums  "
             "oc_limit=%dmA/%ums  hb_timeout=%ums",
             (unsigned)TASK_TICK_MS,
             OVERCURRENT_MA,
             (unsigned)OVERCURRENT_THRESH_MS,
             (unsigned)HEARTBEAT_TIMEOUT_MS);
}
