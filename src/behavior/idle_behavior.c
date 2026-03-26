#include "idle_behavior.h"
#include "state_vector.h"
#include "face_engine.h"
#include "face_params.h"
#include "blink_controller.h"
#include "gaze_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "IDLE";

/* ── Helpers ──────────────────────────────────────────────────────── */

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int8_t clamp8(int v)
{
    return v > 127 ? (int8_t)127 : (v < -128 ? (int8_t)-128 : (int8_t)v);
}

/* Intervalo aleatório em [min_ms, max_ms) */
static inline uint32_t rand_ms(uint32_t min_ms, uint32_t max_ms)
{
    if (max_ms <= min_ms) return min_ms;
    return min_ms + (esp_random() % (max_ms - min_ms));
}

/* ── idle_trigger_t ───────────────────────────────────────────────── */

typedef struct {
    uint32_t    next_fire_ms;
    uint32_t    min_interval_ms;
    uint32_t    max_interval_ms;
    bool        (*condition)(void);   /* NULL = sempre ativo */
    void        (*action)(void);
    const char *name;
} idle_trigger_t;

static void reschedule(idle_trigger_t *t, uint32_t now)
{
    t->next_fire_ms = now + rand_ms(t->min_interval_ms, t->max_interval_ms);
}

/* ═══════════════════════════════════════════════════════════════════
   TIER 2 — Behaviors ocasionais
   ═══════════════════════════════════════════════════════════════════ */

/* ── (a) Olhar para o lado ────────────────────────────────────────── */
static void act_look_side(void)
{
    const float x = (esp_random() & 1u) ? 0.42f : -0.42f;
    gaze_service_set_target(x, 0.0f, 180);
    vTaskDelay(pdMS_TO_TICKS(1700));
    gaze_service_set_target(0.0f, 0.0f, 200);
    vTaskDelay(pdMS_TO_TICKS(300));
}

/* ── (b) Olhar para cima (pensativo) ─────────────────────────────── */
static void act_look_up(void)
{
    /* leve desvio horizontal aleatório */
    const int r3 = (int)(esp_random() % 3u);
    const float x = (r3 == 0) ? 0.2f : (r3 == 1) ? -0.2f : 0.0f;
    gaze_service_set_target(x, -0.28f, 220);
    vTaskDelay(pdMS_TO_TICKS(2100));
    gaze_service_set_target(0.0f, 0.0f, 250);
    vTaskDelay(pdMS_TO_TICKS(350));
}

/* ── (c) Squint pensativo — olhos semicerram ─────────────────────── */
static void act_squint_think(void)
{
    /* Suprime blink automático para evitar race condition:
     * blink_controller captura _dst durante vTaskDelay e sobrescreve o restore. */
    blink_suppress(true);
    vTaskDelay(pdMS_TO_TICKS(250));   /* settle: aguarda blink em andamento terminar */

    face_params_t base;
    face_engine_get_target(&base);

    face_params_t sq  = base;
    sq.open_l         = clampf(base.open_l * 0.52f, 0.05f, 1.0f);
    sq.open_r         = clampf(base.open_r * 0.52f, 0.05f, 1.0f);
    sq.tl_l = sq.tl_r = clamp8((int)base.tl_l + 8);
    sq.tr_l = sq.tr_r = clamp8((int)base.tr_l + 8);
    sq.transition_ms  = 380;
    face_engine_apply_params(&sq);
    vTaskDelay(pdMS_TO_TICKS(950));

    face_params_t restore  = base;
    restore.transition_ms  = 380;
    face_engine_apply_params(&restore);
    vTaskDelay(pdMS_TO_TICKS(450));

    blink_suppress(false);
}

/* ── (d) Blink lento — sonolência (energy < 0.5) ─────────────────── */
static bool cond_slow_blink(void) { return g_state.energy < 0.5f; }

static void act_slow_blink(void)
{
    face_params_t base;
    face_engine_get_target(&base);
    blink_suppress(true);

    /* Fecha metade */
    face_params_t half  = base;
    half.open_l         = clampf(base.open_l * 0.42f, 0.05f, 1.0f);
    half.open_r         = clampf(base.open_r * 0.42f, 0.05f, 1.0f);
    half.transition_ms  = 500;
    face_engine_apply_params(&half);
    vTaskDelay(pdMS_TO_TICKS(650));

    /* Reabre */
    face_params_t restore  = base;
    restore.transition_ms  = 600;
    face_engine_apply_params(&restore);
    vTaskDelay(pdMS_TO_TICKS(700));

    blink_suppress(false);
}

/* ── (e) Double blink — alerta (energy >= 0.5) ───────────────────── */
static bool cond_double_blink(void) { return g_state.energy >= 0.5f; }

static void act_double_blink(void)
{
    blink_controller_trigger();
    vTaskDelay(pdMS_TO_TICKS(400));   /* aguarda 1º blink (~210 ms) + buffer */
    blink_controller_trigger();
    vTaskDelay(pdMS_TO_TICKS(320));   /* aguarda 2º blink concluir */
}

/* ── (f) Sorriso sutil — humor positivo (valence > 0) ────────────── */
static bool cond_slight_smile(void) { return g_state.mood_valence > 0.0f; }

static void act_slight_smile(void)
{
    blink_suppress(true);
    vTaskDelay(pdMS_TO_TICKS(250));   /* settle */

    face_params_t base;
    face_engine_get_target(&base);

    face_params_t smile  = base;
    smile.cv_bot         = clamp8((int)base.cv_bot - 10);
    smile.open_l         = clampf(base.open_l * 0.88f, 0.05f, 1.0f);
    smile.open_r         = clampf(base.open_r * 0.88f, 0.05f, 1.0f);
    smile.transition_ms  = 600;
    face_engine_apply_params(&smile);
    vTaskDelay(pdMS_TO_TICKS(2500));

    face_params_t restore  = base;
    restore.transition_ms  = 600;
    face_engine_apply_params(&restore);
    vTaskDelay(pdMS_TO_TICKS(700));

    blink_suppress(false);
}

/* ── Tabela Tier 2 ────────────────────────────────────────────────── */

static idle_trigger_t s_tier2[] = {
    { 0,  8000,  20000, NULL,              act_look_side,   "look_side"    },
    { 0, 15000,  35000, NULL,              act_look_up,     "look_up"      },
    { 0, 12000,  25000, NULL,              act_squint_think,"squint_think" },
    { 0, 10000,  20000, cond_slow_blink,   act_slow_blink,  "slow_blink"   },
    { 0, 20000,  40000, cond_double_blink, act_double_blink,"double_blink" },
    { 0, 20000,  40000, cond_slight_smile, act_slight_smile,"slight_smile" },
};
#define TIER2_COUNT  (sizeof(s_tier2)  / sizeof(s_tier2[0]))

/* ═══════════════════════════════════════════════════════════════════
   TIER 3 — Animações raras
   ═══════════════════════════════════════════════════════════════════ */

/* Cooldown global: 2 min mínimo entre animações Tier 3 */
static uint32_t s_t3_last_ms = 0u;
#define T3_COOLDOWN_MS  (2u * 60u * 1000u)

/* ── Bocejo ───────────────────────────────────────────────────────── */

#define YAWN_COOLDOWN_MS   (5u * 60u * 1000u)
#define YAWN_IDLE_MIN_MS   (3u * 60u * 1000u)

static uint32_t s_last_yawn_ms = 0u;

static bool cond_yawn(void)
{
    const uint32_t now = now_ms();
    if (s_last_yawn_ms && (now - s_last_yawn_ms) < YAWN_COOLDOWN_MS) return false;
    if (g_state.energy >= 0.5f) return false;
    return (now - g_state.last_interaction_ms) > YAWN_IDLE_MIN_MS;
}

void idle_behavior_trigger_yawn(void)
{
    const uint32_t now = now_ms();
    if (s_last_yawn_ms && (now - s_last_yawn_ms) < YAWN_COOLDOWN_MS) {
        ESP_LOGD(TAG, "yawn cooldown ativo");
        return;
    }
    s_last_yawn_ms = now;
    s_t3_last_ms   = now;
    ESP_LOGI(TAG, "bocejo");

    face_params_t base;
    face_engine_get_target(&base);
    blink_suppress(true);

    /* KF1 2 s: boca abre — cantos inferiores descem, olhos baixam, cv_bot cresce */
    face_params_t kf  = base;
    kf.bl_l = kf.br_l = kf.bl_r = kf.br_r = clamp8((int)base.bl_l + 12);
    kf.y_l            = clamp8((int)base.y_l + 6);
    kf.y_r            = clamp8((int)base.y_r + 6);
    kf.cv_bot         = clamp8((int)base.cv_bot + 14);
    kf.transition_ms  = 2000;
    face_engine_apply_params(&kf);
    vTaskDelay(pdMS_TO_TICKS(2100));

    /* KF2 1 s: pálpebra cai 70% */
    kf.open_l         = clampf(base.open_l * 0.70f, 0.05f, 1.0f);
    kf.open_r         = clampf(base.open_r * 0.70f, 0.05f, 1.0f);
    kf.transition_ms  = 1000;
    face_engine_apply_params(&kf);
    vTaskDelay(pdMS_TO_TICKS(1100));

    /* KF3 1 s: hold no auge do bocejo */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* KF4 2 s: retorna à expressão base */
    face_params_t restore  = base;
    restore.transition_ms  = 2000;
    face_engine_apply_params(&restore);
    vTaskDelay(pdMS_TO_TICKS(2100));

    blink_suppress(false);
    ESP_LOGD(TAG, "bocejo concluído");
}

static void play_yawn(void) { idle_behavior_trigger_yawn(); }

/* ── Espirro ──────────────────────────────────────────────────────── */

#define SNEEZE_COOLDOWN_MS  (8u * 60u * 1000u)
static uint32_t s_last_sneeze_ms = 0u;

static bool cond_sneeze(void)
{
    return !s_last_sneeze_ms ||
           (now_ms() - s_last_sneeze_ms) > SNEEZE_COOLDOWN_MS;
}

static void play_sneeze(void)
{
    s_last_sneeze_ms = now_ms();
    s_t3_last_ms     = now_ms();
    ESP_LOGI(TAG, "espirro");

    face_params_t base;
    face_engine_get_target(&base);
    blink_suppress(true);

    /* KF1 400 ms: buildup nervoso — olhos semicerram, cv_top sobe */
    face_params_t kf  = base;
    kf.open_l         = clampf(base.open_l * 0.62f, 0.05f, 1.0f);
    kf.open_r         = clampf(base.open_r * 0.62f, 0.05f, 1.0f);
    kf.cv_top         = clamp8((int)base.cv_top - 8);
    kf.transition_ms  = 400;
    face_engine_apply_params(&kf);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* KF2 80 ms: snap — olhos fecham, cabeça "vai para frente" (y sobe) */
    kf.open_l         = clampf(base.open_l * 0.07f, 0.05f, 1.0f);
    kf.open_r         = clampf(base.open_r * 0.07f, 0.05f, 1.0f);
    kf.y_l            = clamp8((int)base.y_l + 8);
    kf.y_r            = clamp8((int)base.y_r + 8);
    kf.cv_bot         = clamp8((int)base.cv_bot + 12);
    kf.cv_top         = base.cv_top;
    kf.transition_ms  = 80;
    face_engine_apply_params(&kf);
    vTaskDelay(pdMS_TO_TICKS(300));

    /* KF3 300 ms: embarrassed — reabre a ~55% */
    kf.open_l         = clampf(base.open_l * 0.55f, 0.05f, 1.0f);
    kf.open_r         = clampf(base.open_r * 0.55f, 0.05f, 1.0f);
    kf.cv_bot         = base.cv_bot;
    kf.y_l            = base.y_l;
    kf.y_r            = base.y_r;
    kf.transition_ms  = 300;
    face_engine_apply_params(&kf);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* KF4 500 ms: restaura */
    face_params_t restore  = base;
    restore.transition_ms  = 500;
    face_engine_apply_params(&restore);
    vTaskDelay(pdMS_TO_TICKS(600));

    blink_suppress(false);
    ESP_LOGD(TAG, "espirro concluído");
}

/* ── Soluço ───────────────────────────────────────────────────────── */

#define HICCUP_COOLDOWN_MS  (5u * 60u * 1000u)
static uint32_t s_last_hiccup_ms = 0u;

static bool cond_hiccup(void)
{
    return !s_last_hiccup_ms ||
           (now_ms() - s_last_hiccup_ms) > HICCUP_COOLDOWN_MS;
}

static void play_hiccup(void)
{
    s_last_hiccup_ms = now_ms();
    s_t3_last_ms     = now_ms();
    ESP_LOGI(TAG, "soluço");

    face_params_t base;
    face_engine_get_target(&base);

    /* 2–4 soluços */
    const uint8_t reps = 2u + (uint8_t)(esp_random() % 3u);

    for (uint8_t i = 0; i < reps; i++) {
        /* Micro-susto: olhos abrem levemente e sobem */
        face_params_t kf  = base;
        kf.open_l         = clampf(base.open_l * 1.18f, 0.05f, 1.0f);
        kf.open_r         = clampf(base.open_r * 1.18f, 0.05f, 1.0f);
        kf.y_l            = clamp8((int)base.y_l - 5);
        kf.y_r            = clamp8((int)base.y_r - 5);
        kf.transition_ms  = 60;
        face_engine_apply_params(&kf);
        vTaskDelay(pdMS_TO_TICKS(150));

        /* Retorna */
        face_params_t restore  = base;
        restore.transition_ms  = 200;
        face_engine_apply_params(&restore);
        vTaskDelay(pdMS_TO_TICKS(rand_ms(350u, 650u)));
    }
    ESP_LOGD(TAG, "soluço concluído (%u reps)", (unsigned)reps);
}

/* ── Tabela Tier 3 ────────────────────────────────────────────────── */

static idle_trigger_t s_tier3[] = {
    { 0,  300000,  900000, cond_yawn,   play_yawn,   "yawn"   },
    { 0,  600000, 1800000, cond_sneeze, play_sneeze, "sneeze" },
    { 0,  480000, 1200000, cond_hiccup, play_hiccup, "hiccup" },
};
#define TIER3_COUNT  (sizeof(s_tier3) / sizeof(s_tier3[0]))

/* ═══════════════════════════════════════════════════════════════════
   IdleTask
   ═══════════════════════════════════════════════════════════════════ */

static void idle_task(void *arg)
{
    (void)arg;

    /* Escalonar todos os timers escalonados a partir do boot +5 s,
     * distribuídos aleatoriamente para evitar rafada inicial. */
    const uint32_t boot_ref = now_ms() + 5000u;

    for (size_t i = 0; i < TIER2_COUNT; i++) {
        s_tier2[i].next_fire_ms = boot_ref +
            rand_ms(s_tier2[i].min_interval_ms, s_tier2[i].max_interval_ms);
    }
    for (size_t i = 0; i < TIER3_COUNT; i++) {
        s_tier3[i].next_fire_ms = boot_ref +
            rand_ms(s_tier3[i].min_interval_ms, s_tier3[i].max_interval_ms);
    }

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(100));   /* 10 Hz */

        const uint32_t now = now_ms();

        /* ── Tier 3 ─── uma animação rara por vez, cooldown global ── */
        if (now - s_t3_last_ms > T3_COOLDOWN_MS) {
            for (size_t i = 0; i < TIER3_COUNT; i++) {
                idle_trigger_t *t = &s_tier3[i];
                if (now < t->next_fire_ms) continue;
                reschedule(t, now);
                if (t->condition && !t->condition()) continue;
                ESP_LOGI(TAG, "tier3: %s", t->name);
                t->action();   /* bloqueia enquanto a animação roda */
                break;
            }
        }

        /* ── Tier 2 ─── um behavior por ciclo de 100 ms ─────────── */
        for (size_t i = 0; i < TIER2_COUNT; i++) {
            idle_trigger_t *t = &s_tier2[i];
            if (now < t->next_fire_ms) continue;
            reschedule(t, now);
            if (t->condition && !t->condition()) continue;
            ESP_LOGD(TAG, "tier2: %s", t->name);
            t->action();   /* bloqueia durante a ação */
            break;
        }
    }
}

/* ── Init ─────────────────────────────────────────────────────────── */

void idle_behavior_init(void)
{
    xTaskCreatePinnedToCore(idle_task, "IdleTask",
                            4096, NULL, 3, NULL, 1);
    ESP_LOGI(TAG, "IdleTask Core 1 pri 3  tier2=%d  tier3=%d",
             (int)TIER2_COUNT, (int)TIER3_COUNT);
}
