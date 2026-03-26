#include "behavior_engine.h"
#include "behavior_tree.h"
#include "state_vector.h"
#include "emotion_mapper.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "BENG";

/* ── Helpers ────────────────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Estado FSM ─────────────────────────────────────────────────────── */

static volatile behavior_state_t s_state          = BSTATE_IDLE;
static volatile bool             s_safe_mode_req  = false;
static volatile bool             s_wake_word_req  = false;
static volatile int64_t          s_safe_exit_us   = 0;  /* µs de saída do SAFE_MODE */

#define SAFE_MODE_DURATION_US  (5000000LL)   /* 5 s */
#define TALKING_ATTN_EXIT      0.85f         /* atenção abaixo disto → ENGAGED */

/* ── Rastreio de emoção atual ────────────────────────────────────────── */

static emotion_t s_cur_emotion    = EMOTION_NEUTRAL;
static uint32_t  s_emotion_age_ms = 0;   /* ms desde a última aplicação */

#define EMOTION_REFRESH_MS  5000u   /* reaplicar mesmo sem mudança a cada 5 s */

static void apply_emotion(emotion_t e, uint16_t ms)
{
    if (e != s_cur_emotion || s_emotion_age_ms >= EMOTION_REFRESH_MS) {
        s_cur_emotion    = e;
        s_emotion_age_ms = 0;
        emotion_mapper_apply(e, ms);
    }
}

/* ── Callbacks de evento (executam na task do EventBus) ─────────────── */

static void on_touch(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    g_state.mood_arousal = clampf(g_state.mood_arousal + 0.3f, 0.0f, 1.0f);
    state_vector_on_interaction();
}

static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    g_state.attention = 1.0f;
    s_wake_word_req   = true;
    state_vector_on_interaction();
}

static void on_lowbat(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    g_state.energy = clampf(g_state.energy - 0.2f, 0.0f, 1.0f);
    ESP_LOGW(TAG, "LOWBAT — energy=%.2f", g_state.energy);
}

static void on_servo_blocked(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    s_safe_mode_req = true;
}

/* ── FSM tick ───────────────────────────────────────────────────────── */

static void fsm_tick(void)
{
    /* SAFE_MODE: sobrepõe qualquer estado — tratado primeiro */
    if (s_safe_mode_req) {
        s_safe_mode_req = false;
        s_safe_exit_us  = esp_timer_get_time() + SAFE_MODE_DURATION_US;
        if (s_state != BSTATE_SAFE_MODE) {
            ESP_LOGW(TAG, "→ SAFE_MODE (servo bloqueado)");
        }
        s_state = BSTATE_SAFE_MODE;
        return;
    }

    switch (s_state) {

        case BSTATE_SAFE_MODE:
            if (esp_timer_get_time() >= s_safe_exit_us) {
                ESP_LOGI(TAG, "SAFE_MODE expirou → IDLE");
                s_state = BSTATE_IDLE;
            }
            break;

        case BSTATE_SLEEP:
            if (g_state.energy >= 0.15f || g_state.attention > 0.3f) {
                ESP_LOGI(TAG, "SLEEP → IDLE  energy=%.2f", g_state.energy);
                s_state = BSTATE_IDLE;
            }
            break;

        case BSTATE_IDLE:
            if (g_state.energy < 0.15f) {
                ESP_LOGI(TAG, "IDLE → SLEEP  energy=%.2f", g_state.energy);
                s_state = BSTATE_SLEEP;
            } else if (g_state.attention > 0.5f || g_state.person_present) {
                ESP_LOGI(TAG, "IDLE → ENGAGED  attn=%.2f", g_state.attention);
                s_state = BSTATE_ENGAGED;
            }
            break;

        case BSTATE_ENGAGED:
            if (s_wake_word_req) {
                s_wake_word_req = false;
                ESP_LOGI(TAG, "ENGAGED → TALKING  attn=%.2f", g_state.attention);
                s_state = BSTATE_TALKING;
            } else if (g_state.attention < 0.3f && !g_state.person_present) {
                ESP_LOGI(TAG, "ENGAGED → IDLE  attn=%.2f", g_state.attention);
                s_state = BSTATE_IDLE;
            }
            break;

        case BSTATE_TALKING:
            if (g_state.attention < TALKING_ATTN_EXIT) {
                ESP_LOGI(TAG, "TALKING → ENGAGED  attn=%.2f", g_state.attention);
                s_state = BSTATE_ENGAGED;
            }
            break;

        default:
            s_state = BSTATE_IDLE;
            break;
    }
}

/* ── Nós folha da behavior tree ─────────────────────────────────────── */

static bt_status_t node_safe_mode(void)
{
    if (s_state != BSTATE_SAFE_MODE) return BT_FAILURE;
    apply_emotion(EMOTION_SCARED, 150);
    return BT_SUCCESS;
}

static bt_status_t node_talking(void)
{
    if (s_state != BSTATE_TALKING) return BT_FAILURE;
    apply_emotion(EMOTION_HAPPY, 200);
    return BT_SUCCESS;
}

static bt_status_t node_engaged(void)
{
    if (s_state != BSTATE_ENGAGED) return BT_FAILURE;
    emotion_t e = (g_state.mood_valence > 0.2f) ? EMOTION_HAPPY : EMOTION_NEUTRAL;
    apply_emotion(e, 300);
    return BT_SUCCESS;
}

static bt_status_t node_sleep(void)
{
    if (s_state != BSTATE_SLEEP) return BT_FAILURE;
    apply_emotion(EMOTION_SAD, 600);
    return BT_SUCCESS;
}

static bt_status_t node_idle(void)
{
    /* Fallback — sempre retorna SUCCESS */
    emotion_t e;
    if (g_state.energy < 0.30f) {
        e = EMOTION_SAD;
    } else if (g_state.mood_valence < -0.20f) {
        e = EMOTION_SAD;
    } else {
        e = EMOTION_NEUTRAL;
    }
    apply_emotion(e, 400);
    return BT_SUCCESS;
}

/* ── Raiz da árvore ─────────────────────────────────────────────────── */

static const bt_node_fn k_root_nodes[] = {
    node_safe_mode,   /* pri 1 — segurança crítica        */
    node_talking,     /* pri 2 — interação por voz        */
    node_engaged,     /* pri 3 — presença / atenção alta  */
    node_sleep,       /* pri 4 — energia muito baixa      */
    node_idle,        /* pri 5 — fallback                 */
};
#define ROOT_COUNT (sizeof(k_root_nodes) / sizeof(k_root_nodes[0]))

static void behavior_tree_tick(void)
{
    bt_selector(k_root_nodes, ROOT_COUNT);
}

/* ── BehaviorLoopTask ────────────────────────────────────────────────── */

static void behavior_loop_task(void *arg)
{
    (void)arg;

    TickType_t xLastWake = xTaskGetTickCount();
    uint8_t    log_div   = 0;

    for (;;) {
        s_emotion_age_ms += 100u;   /* avança contador antes do tick */

        fsm_tick();
        behavior_tree_tick();

        /* Log a cada 10 ticks (1 s) em nível DEBUG */
        if (++log_div >= 10u) {
            log_div = 0;
            ESP_LOGD(TAG, "[%s] energy=%.2f attn=%.2f arousal=%.2f valence=%.2f",
                     behavior_state_name(s_state),
                     g_state.energy, g_state.attention,
                     g_state.mood_arousal, g_state.mood_valence);
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(100));
    }
}

/* ── API pública ────────────────────────────────────────────────────── */

void behavior_engine_init(void)
{
    event_bus_subscribe(EVT_TOUCH_DETECTED, on_touch);
    event_bus_subscribe(EVT_WAKE_WORD,      on_wake_word);
    event_bus_subscribe(EVT_SYS_LOWBAT,    on_lowbat);
    event_bus_subscribe(EVT_SERVO_BLOCKED,  on_servo_blocked);

    xTaskCreatePinnedToCore(behavior_loop_task, "BehaviorLoop",
                            4096, NULL, 12, NULL, 1);

    ESP_LOGI(TAG, "init  BehaviorLoopTask Core 1 pri 12 tick=100ms");
}

behavior_state_t behavior_engine_get_state(void)
{
    return s_state;
}

const char *behavior_state_name(behavior_state_t s)
{
    static const char *names[BSTATE_COUNT] = {
        "SLEEP", "IDLE", "ENGAGED", "TALKING", "SAFE_MODE",
    };
    return (s < BSTATE_COUNT) ? names[s] : "UNKNOWN";
}
