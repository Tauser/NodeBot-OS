#include "behavior_engine.h"
#include "behavior_tree.h"
#include "state_vector.h"
#include "motion_safety_service.h"
#include "dialogue_state_service.h"
#include "safe_mode_service.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>

static const char *TAG = "behavior";

/* ── Configuração ──────────────────────────────────────────────────────── */
#define TASK_STACK         3072u
#define TASK_PRIO            12u
#define TICK_MS             100u

/* Timeouts de FSM */
#define ENGAGED_TIMEOUT_MS  30000u   /* 30 s sem interação → IDLE  */
#define SLEEP_ENERGY_THR     0.15f   /* energy abaixo → SLEEP      */

/* ── Estado compartilhado (atualizado por callbacks, lido na task) ───── */
static volatile uint32_t s_last_wake_word_ms   = 0u;
static volatile uint32_t s_last_touch_ms       = 0u;
static volatile uint32_t s_last_interaction_ms = 0u;

/* ── FSM ──────────────────────────────────────────────────────────────── */
static engine_state_t    s_state = ENGINE_IDLE;
static SemaphoreHandle_t s_mutex = NULL;

static void set_state(engine_state_t new_state)
{
    if (s_state == new_state) return;
    ESP_LOGI(TAG, "FSM %d → %d", (int)s_state, (int)new_state);
    s_state = new_state;
}

/* ── Transições FSM ────────────────────────────────────────────────────── */

static void fsm_update(uint32_t now_ms)
{
    /* P0: safe mode sobrepõe tudo */
    if (safe_mode_is_active()) {
        set_state(ENGINE_SAFE_MODE);
        return;
    }
    if (s_state == ENGINE_SAFE_MODE) {
        set_state(ENGINE_IDLE);
    }

    /* P1: diálogo ativo → TALKING */
    dialogue_state_t dlg = dialogue_state_get();
    if (dlg == DIALOGUE_LISTENING || dlg == DIALOGUE_PROCESSING ||
        dlg == DIALOGUE_SPEAKING) {
        if (s_state != ENGINE_TALKING) set_state(ENGINE_TALKING);
        return;
    }

    /* Voltando de TALKING */
    if (s_state == ENGINE_TALKING) {
        set_state(ENGINE_ENGAGED);
        s_last_interaction_ms = now_ms;
    }

    /* P2: transições SLEEP / IDLE */
    if (s_state == ENGINE_SLEEP) {
        /* Acorda com interação */
        if (s_last_wake_word_ms != 0u &&
            (now_ms - s_last_wake_word_ms) < 5000u) {
            set_state(ENGINE_ENGAGED);
        } else if (s_last_touch_ms != 0u &&
                   (now_ms - s_last_touch_ms) < 2000u) {
            set_state(ENGINE_ENGAGED);
        }
        return;
    }

    /* P3: IDLE → SLEEP se muito cansado e sem interação por 10 min */
    if (s_state == ENGINE_IDLE) {
        if (g_state.energy < SLEEP_ENERGY_THR &&
            (now_ms - s_last_interaction_ms) > 600000u) {
            set_state(ENGINE_SLEEP);
        }
        /* IDLE → ENGAGED com wake word ou toque */
        else if (s_last_wake_word_ms != 0u &&
                 (now_ms - s_last_wake_word_ms) < 5000u) {
            set_state(ENGINE_ENGAGED);
        } else if (s_last_touch_ms != 0u &&
                   (now_ms - s_last_touch_ms) < 2000u) {
            set_state(ENGINE_ENGAGED);
        }
        return;
    }

    /* P4: ENGAGED → IDLE após timeout */
    if (s_state == ENGINE_ENGAGED) {
        if ((now_ms - s_last_interaction_ms) > ENGAGED_TIMEOUT_MS) {
            set_state(ENGINE_IDLE);
        }
    }
}

/* ── BehaviorLoopTask ──────────────────────────────────────────────────── */

static void behavior_loop_task(void *arg)
{
    (void)arg;
    TickType_t  last_wake     = xTaskGetTickCount();
    uint32_t    tick_count    = 0u;

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TICK_MS));

        /* ── 1. Heartbeat (SEMPRE PRIMEIRO) ─────────────────────────── */
        motion_safety_feed_heartbeat();

        /* ── 2. StateVector decay ────────────────────────────────────── */
        state_vector_tick(TICK_MS);

        /* ── 3. Eventos já processados via callbacks (sem bloqueio) ──── */
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);

        /* ── 4. FSM update ───────────────────────────────────────────── */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        fsm_update(now_ms);
        xSemaphoreGive(s_mutex);

        /* ── 5. Behavior tree ────────────────────────────────────────── */
        bt_context_t ctx = {
            .now_ms            = now_ms,
            .last_wake_word_ms = s_last_wake_word_ms,
            .last_touch_ms     = s_last_touch_ms,
            .safe_mode_active  = (s_state == ENGINE_SAFE_MODE),
            .battery_pct       = g_state.battery_pct,
        };
        behavior_tree_evaluate(&ctx);

        /* ── 5b. Mood derivado do StateVector (EMO_SPEC §3) — a cada 1s ─ */
        if (tick_count % 10u == 0u) {
            behavior_tree_apply_mood();
        }

        /* ── 6. Log periódico (a cada 60 s) ─────────────────────────── */
        tick_count++;
        if (tick_count % 600u == 0u) {
            ESP_LOGI(TAG, "state=%d energy=%.2f arousal=%.2f attention=%.2f",
                     (int)s_state,
                     (double)g_state.energy,
                     (double)g_state.mood_arousal,
                     (double)g_state.attention);
        }
    }
}

/* ── Callbacks do EventBus ─────────────────────────────────────────────── */

static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
    s_last_wake_word_ms   = now_ms;
    s_last_interaction_ms = now_ms;
    /* Atenção total */
    g_state.attention = 1.0f;
    state_vector_on_interaction();
}

static void on_touch_press(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
    s_last_touch_ms       = now_ms;
    s_last_interaction_ms = now_ms;
    /* Arousal sobe com toque */
    float a = g_state.mood_arousal + 0.3f;
    g_state.mood_arousal = (a > 1.0f) ? 1.0f : a;
    state_vector_on_touch(false);
}

static void on_intent_detected(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);
    s_last_interaction_ms = now_ms;
    state_vector_on_interaction();
}

static void on_lowbat(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    float e = g_state.energy - 0.2f;
    g_state.energy = (e < 0.0f) ? 0.0f : e;
}

/* ── API pública ────────────────────────────────────────────────────────── */

engine_state_t behavior_engine_get_state(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    engine_state_t st = s_state;
    xSemaphoreGive(s_mutex);
    return st;
}

esp_err_t behavior_engine_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "falha ao criar mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = behavior_tree_init();
    if (err != ESP_OK) {
        vSemaphoreDelete(s_mutex);
        return err;
    }

    /* Subscrições */
    err  = event_bus_subscribe(EVT_WAKE_WORD,       on_wake_word);
    err |= event_bus_subscribe(EVT_TOUCH_PRESS,     on_touch_press);
    err |= event_bus_subscribe(EVT_INTENT_DETECTED, on_intent_detected);
    err |= event_bus_subscribe(EVT_SYS_LOWBAT,      on_lowbat);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_mutex);
        return err;
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        behavior_loop_task, "behavior",
        TASK_STACK, NULL, TASK_PRIO, NULL, 1 /* Core 1 */);

    if (rc != pdPASS) {
        vSemaphoreDelete(s_mutex);
        ESP_LOGE(TAG, "falha ao criar task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ok — P%u Core1 tick=%ums", TASK_PRIO, TICK_MS);
    return ESP_OK;
}
