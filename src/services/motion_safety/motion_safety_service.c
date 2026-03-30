#include "motion_safety_service.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>

static const char *TAG = "SAFETY";

/* ── Constantes ──────────────────────────────────────────────────────── */
#define SAFETY_TASK_PERIOD_MS   5u
#define OVERCURRENT_MA          800u
#define OVERCURRENT_DEBOUNCE_MS 80u
#define HEARTBEAT_TIMEOUT_MS    500u
#define NUM_SERVOS              2u

/*
 * Interface do driver de servo (E04).
 * Declaradas como weak — retornam 0 / não-op até E04 ser integrado.
 */
__attribute__((weak)) int scs0009_get_current_ma(uint8_t id)  { (void)id; return 0; }
__attribute__((weak)) void scs0009_set_torque_enable(uint8_t id, bool en)
{
    (void)id; (void)en;
}

/* ── Estado interno (sem malloc) ─────────────────────────────────────── */
static portMUX_TYPE      s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool     s_safe          = true;
static volatile uint32_t s_last_hb_ms    = 0u;

/* Acumuladores de debounce por servo (contados em ticks de 5 ms) */
static uint32_t          s_overcurrent_accum_ms[NUM_SERVOS];

/* ── Helpers ─────────────────────────────────────────────────────────── */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void stop_all_servos(void)
{
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        scs0009_set_torque_enable(i, false);
        s_overcurrent_accum_ms[i] = 0u;
    }
}

static void trigger_blocked(uint8_t servo_id)
{
    taskENTER_CRITICAL(&s_mux);
    s_safe = false;
    taskEXIT_CRITICAL(&s_mux);

    stop_all_servos();

    uint8_t id = servo_id;
    event_bus_publish(EVT_SERVO_BLOCKED, &id, sizeof(id), EVENT_PRIO_SAFETY);
    ESP_LOGE(TAG, "SERVO BLOCKED  id=%u  → torque off", servo_id);
}

/* ── MotionSafetyTask ────────────────────────────────────────────────── */
static void safety_task(void *arg)
{
    (void)arg;
    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        const uint32_t now = now_ms();

        /* (1) Verificar heartbeat ──────────────────────────────────────
         *   Se o BehaviorLoop parar de alimentar por > HEARTBEAT_TIMEOUT_MS,
         *   considera falha e para os servos. */
        {
            uint32_t last_hb;
            taskENTER_CRITICAL(&s_mux);
            last_hb = s_last_hb_ms;
            taskEXIT_CRITICAL(&s_mux);

            if (last_hb != 0u && (now - last_hb) > HEARTBEAT_TIMEOUT_MS) {
                if (s_safe) {
                    taskENTER_CRITICAL(&s_mux);
                    s_safe = false;
                    taskEXIT_CRITICAL(&s_mux);
                    stop_all_servos();
                    ESP_LOGE(TAG, "HEARTBEAT TIMEOUT → servos parados");
                    uint8_t id = 0xFF;  /* indica timeout, não servo específico */
                    event_bus_publish(EVT_SERVO_BLOCKED, &id, sizeof(id),
                                      EVENT_PRIO_SAFETY);
                }
            }
        }

        /* (2) Verificar corrente por servo ─────────────────────────────
         *   Debounce: acumula tempo acima do limite; só bloqueia após 80 ms. */
        for (uint8_t i = 0; i < NUM_SERVOS; i++) {
            int ma = scs0009_get_current_ma(i);
            if (ma > (int)OVERCURRENT_MA) {
                s_overcurrent_accum_ms[i] += SAFETY_TASK_PERIOD_MS;
                if (s_overcurrent_accum_ms[i] >= OVERCURRENT_DEBOUNCE_MS) {
                    trigger_blocked(i);
                    /* Reseta acumuladores de todos os servos após bloqueio */
                    memset(s_overcurrent_accum_ms, 0, sizeof(s_overcurrent_accum_ms));
                }
            } else {
                s_overcurrent_accum_ms[i] = 0u;
            }
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(SAFETY_TASK_PERIOD_MS));
    }
}

/* ── API pública ─────────────────────────────────────────────────────── */

void motion_safety_init(void)
{
    memset(s_overcurrent_accum_ms, 0, sizeof(s_overcurrent_accum_ms));
    s_safe       = true;
    s_last_hb_ms = 0u;

    xTaskCreatePinnedToCore(
        safety_task,
        "MotionSafetyTask",
        2048,       /* stack mínimo — sem malloc, sem I/O */
        NULL,
        22,         /* P22 — maior prioridade da app */
        NULL,
        1           /* Core 1 */
    );

    ESP_LOGI(TAG, "MotionSafetyTask  Core 1  P22  tick=%u ms", SAFETY_TASK_PERIOD_MS);
}

void motion_safety_feed_heartbeat(void)
{
    const uint32_t now = now_ms();
    taskENTER_CRITICAL(&s_mux);
    s_last_hb_ms = now;
    /* Ao receber heartbeat, restaura estado seguro se havia timeout
     * (overcurrent continua bloqueado — só reseta por reset de HW) */
    taskEXIT_CRITICAL(&s_mux);
}

void motion_safety_emergency_stop(void)
{
    taskENTER_CRITICAL(&s_mux);
    s_safe = false;
    taskEXIT_CRITICAL(&s_mux);

    stop_all_servos();

    uint8_t id = 0xFF;
    event_bus_publish(EVT_SERVO_BLOCKED, &id, sizeof(id), EVENT_PRIO_SAFETY);
    ESP_LOGE(TAG, "EMERGENCY STOP chamado");
}

bool motion_safety_is_safe(void)
{
    bool v;
    taskENTER_CRITICAL(&s_mux);
    v = s_safe;
    taskEXIT_CRITICAL(&s_mux);
    return v;
}
