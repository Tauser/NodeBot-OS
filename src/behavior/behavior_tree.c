#include "behavior_tree.h"
#include "state_vector.h"
#include "face_engine.h"
#include "face_params.h"

#include "esp_log.h"

static const char *TAG = "bt";

/* ── Limiares ──────────────────────────────────────────────────────────── */
#define WAKE_ENGAGE_MS    5000u    /* wake word conta como engajamento por 5s  */
#define TOUCH_REACT_MS    2000u    /* toque conta como reação por 2s           */
#define BATTERY_CRITICAL  10.0f   /* abaixo → cansado + modo low-power        */
#define BATTERY_MIN_PCT    0.1f   /* evita falso-positivo quando sem leitura   */

/* ── Nós ────────────────────────────────────────────────────────────────── */

/* P1 SAFETY — safe mode: face neutra fixada com prioridade máxima */
static bt_result_t node_safe_mode(const bt_context_t *ctx)
{
    if (!ctx->safe_mode_active) return BT_FAILURE;
    face_params_t p = FACE_NEUTRAL;
    p.priority      = FACE_PRIORITY_SAFETY;
    p.transition_ms = 300u;
    face_engine_apply_params(&p);
    ESP_LOGD(TAG, "P1 safe_mode → NEUTRAL/SAFETY");
    return BT_SUCCESS;
}

/* P2 SYSTEM — bateria crítica: face TIRED com prioridade SYSTEM */
static bt_result_t node_battery_low(const bt_context_t *ctx)
{
    if (ctx->battery_pct >= BATTERY_CRITICAL ||
        ctx->battery_pct < BATTERY_MIN_PCT) {
        return BT_FAILURE;
    }
    /* Só aplica na transição e a cada 10 s para não spam */
    static uint32_t s_last_applied_ms = 0u;
    if (ctx->now_ms - s_last_applied_ms < 10000u &&
        s_last_applied_ms != 0u) {
        return BT_SUCCESS;  /* já ativo, mantém estado */
    }
    s_last_applied_ms = ctx->now_ms;

    face_params_t p = FACE_TIRED;     /* FACE_FRUS_BORED */
    p.priority      = FACE_PRIORITY_SYSTEM;
    p.transition_ms = 1000u;
    face_engine_apply_params(&p);
    ESP_LOGD(TAG, "P2 battery=%.0f%% → TIRED/SYSTEM", (double)ctx->battery_pct);
    return BT_SUCCESS;
}

/* P3 REACTION — wake word recente: SURPRISED */
static bt_result_t node_wake_word_recent(const bt_context_t *ctx)
{
    if (ctx->last_wake_word_ms == 0u) return BT_FAILURE;
    if ((ctx->now_ms - ctx->last_wake_word_ms) > WAKE_ENGAGE_MS) return BT_FAILURE;

    /* Aplica apenas na transição (evita reemissão a cada tick de 100ms) */
    static uint32_t s_last_wake_ms = 0u;
    if (s_last_wake_ms == ctx->last_wake_word_ms) return BT_FAILURE;
    s_last_wake_ms = ctx->last_wake_word_ms;

    /* FACE_SURPRISED já tem FACE_PRIORITY_REACTION */
    face_params_t p = FACE_SURPRISED;
    face_engine_apply_params(&p);
    ESP_LOGD(TAG, "P3 wake_word → SURPRISED/REACTION");
    return BT_SUCCESS;
}

/* P4 REACTION — toque recente: HAPPY */
static bt_result_t node_touch_recent(const bt_context_t *ctx)
{
    if (ctx->last_touch_ms == 0u) return BT_FAILURE;
    if ((ctx->now_ms - ctx->last_touch_ms) > TOUCH_REACT_MS) return BT_FAILURE;

    static uint32_t s_last_touch_ms = 0u;
    if (s_last_touch_ms == ctx->last_touch_ms) return BT_FAILURE;
    s_last_touch_ms = ctx->last_touch_ms;

    /* FACE_HAPPY tem FACE_PRIORITY_MOOD; sobe para REACTION aqui */
    face_params_t p = FACE_HAPPY;
    p.priority      = FACE_PRIORITY_REACTION;
    p.transition_ms = 200u;
    face_engine_apply_params(&p);
    ESP_LOGD(TAG, "P4 touch → HAPPY/REACTION");
    return BT_SUCCESS;
}

/* ── Tabela de nós ──────────────────────────────────────────────────────── */
typedef bt_result_t (*bt_node_fn_t)(const bt_context_t *);

static const bt_node_fn_t k_nodes[] = {
    node_safe_mode,
    node_battery_low,
    node_wake_word_recent,
    node_touch_recent,
};
#define N_NODES (sizeof(k_nodes) / sizeof(k_nodes[0]))

/* ── Mood derivado do StateVector (EMO_SPEC §3) ──────────────────────────
 * Chamado pelo BehaviorEngine a cada 1 s.
 * Aplica expressão base em FACE_PRIORITY_MOOD — camadas de maior prioridade
 * (REACTION, SYSTEM, SAFETY) sobrepõem sem serem afetadas. */
void behavior_tree_apply_mood(void)
{
    face_params_t p = FACE_NEUTRAL;
    p.priority      = FACE_PRIORITY_MOOD;
    p.transition_ms = 800u;

    float open = 0.4f + g_state.energy * 0.5f;
    if (open < 0.15f) open = 0.15f;
    if (open > 1.00f) open = 1.00f;
    p.open_l = p.open_r = open;

    float sq = g_state.mood_arousal * 0.15f;
    if (sq < 0.0f) sq = 0.0f;
    if (sq > 0.55f) sq = 0.55f;
    p.squint_l = p.squint_r = sq;

    face_engine_apply_params(&p);
}

/* ── API ─────────────────────────────────────────────────────────────────── */

esp_err_t behavior_tree_init(void)
{
    ESP_LOGI(TAG, "ok — %u nós", (unsigned)N_NODES);
    return ESP_OK;
}

bt_result_t behavior_tree_evaluate(const bt_context_t *ctx)
{
    for (size_t i = 0u; i < N_NODES; i++) {
        if (k_nodes[i](ctx) == BT_SUCCESS) return BT_SUCCESS;
    }
    return BT_FAILURE;   /* idle_behavior task roda autonomamente */
}
