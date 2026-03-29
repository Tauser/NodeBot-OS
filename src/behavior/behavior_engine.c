#include "behavior_engine.h"
#include "behavior_tree.h"
#include "gesture_service.h"
#include "persona_service.h"
#include "state_vector.h"
#include "face_command.h"
#include "reaction_resolver.h"
#include "face_engine.h"
#include "face_params.h"
#include "blink_controller.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "BENG";

/* ── Helpers ─────────────────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── Estado FSM ──────────────────────────────────────────────────────── */

static volatile behavior_state_t s_state         = BSTATE_IDLE;
static volatile bool             s_safe_mode_req = false;
static volatile bool             s_wake_word_req = false;
static volatile bool             s_sleep_req     = false;
static volatile bool             s_wake_req      = false;
static volatile int64_t          s_safe_exit_us  = 0;

#define SAFE_MODE_DURATION_US  (5000000LL)
#define TALKING_ATTN_EXIT      0.85f

/* ── Rastreio de expressão por tag numérico ──────────────────────────── *
 * Cada preset recebe um tag único — face só é re-aplicada se o tag mudar.
 * Evita resetar a transição a cada tick quando a expressão não muda.
 * ──────────────────────────────────────────────────────────────────────*/

static uint8_t s_face_tag = 0xFF;
static bool    s_face_force_apply = false;
static bool    s_baseline_dirty = false;
static face_command_t s_baseline_cmd;
static reaction_resolver_t s_reaction_resolver;
static portMUX_TYPE s_reaction_req_lock = portMUX_INITIALIZER_UNLOCKED;
static face_command_t s_pending_reaction_cmds[REACTION_CATEGORY_COUNT];
static bool s_pending_reaction_submit[REACTION_CATEGORY_COUNT];
static bool s_pending_reaction_clear[REACTION_CATEGORY_COUNT];

/* Tags fixos (um por estado de sistema) */
#define TAG_SAFE_MODE      1
#define TAG_SYSTEM_ALERT   2
#define TAG_SLEEP          3
#define TAG_TALKING        4
#define TAG_MUSIC          5
/* Tags dinâmicos — ENGAGED seleciona por StateVector */
#define TAG_ENGAGED_AWE    10   /* arousal alto + pessoa presente → maravilhado */
#define TAG_ENGAGED_GLEE   11   /* valence > 0.5 → muito feliz                  */
#define TAG_ENGAGED_HAPPY  12   /* valence > 0.25                               */
#define TAG_ENGAGED_FOCUS  13   /* attention > 0.80                             */
#define TAG_ENGAGED_SAD    14   /* valence < -0.25                              */
#define TAG_ENGAGED_NEUT   15   /* padrão                                       */

/* Tags dinâmicos — IDLE seleciona por StateVector */
#define TAG_IDLE_SLEEPY    20   /* energy < 0.20                                */
#define TAG_IDLE_UNIMP     21   /* energy < 0.35                                */
#define TAG_IDLE_GLEE      22   /* valence > 0.60                               */
#define TAG_IDLE_HAPPY     23   /* valence > 0.35                               */
#define TAG_IDLE_SAD_D     24   /* valence < -0.25                              */
#define TAG_IDLE_SAD_U     25   /* valence < -0.10                              */
#define TAG_IDLE_FOCUS     26   /* attention > 0.70                             */
#define TAG_IDLE_AWE       27   /* social_need > 0.60 → procurando pessoa       */
#define TAG_IDLE_NEUTRAL   28   /* padrão                                       */

/* Tags TALKING */
#define TAG_TALK_LISTEN    30   /* attention alta → FOCUSED (ouvindo)           */
#define TAG_TALK_THINK     31   /* attention média → SQUINT (processando)       */

static bool tag_is_sleep_like(uint8_t tag)
{
    return tag == TAG_SLEEP || tag == TAG_IDLE_SLEEPY || tag == TAG_IDLE_UNIMP;
}

static face_params_t normalize_baseline_face(uint8_t tag, face_params_t p)
{
    if (!tag_is_sleep_like(tag)) {
        if (p.open_l < 0.52f) p.open_l = 0.52f;
        if (p.open_r < 0.52f) p.open_r = 0.52f;
        if (p.x_off < 104u) p.x_off = 104u;
    }

    return p;
}

static face_params_t normalize_runtime_face(uint8_t tag, face_params_t p)
{
    if (!tag_is_sleep_like(tag)) {
        if (p.open_l < 0.34f) p.open_l = 0.34f;
        if (p.open_r < 0.34f) p.open_r = 0.34f;
        if (p.x_off < 108u) p.x_off = 108u;
    }

    return p;
}

static face_params_t make_tuned_happy(face_params_t p, float open, uint8_t x_off,
                                      uint16_t transition_ms)
{
    p.open_l = open;
    p.open_r = open;
    p.x_off = x_off;
    p.transition_ms = transition_ms;
    return p;
}

static face_params_t make_tuned_focus(face_params_t p, float open, uint8_t x_off,
                                      int8_t cv_bot, uint16_t transition_ms)
{
    p.open_l = open;
    p.open_r = open;
    p.x_off = x_off;
    p.cv_bot = cv_bot;
    p.transition_ms = transition_ms;
    return p;
}

static face_params_t make_tuned_talk_focus(face_params_t p, float open,
                                           uint8_t x_off, int8_t cv_top,
                                           int8_t cv_bot,
                                           uint16_t transition_ms)
{
    p.open_l = open;
    p.open_r = open;
    p.x_off = x_off;
    p.cv_top = cv_top;
    p.cv_bot = cv_bot;
    p.transition_ms = transition_ms;
    return p;
}

static face_params_t make_tuned_neutral(face_params_t p, float open,
                                        uint8_t x_off, uint16_t transition_ms)
{
    p.open_l = open;
    p.open_r = open;
    p.x_off = x_off;
    p.transition_ms = transition_ms;
    return p;
}

static const char *face_tag_name(uint8_t tag)
{
    switch (tag) {
        case TAG_SAFE_MODE:    return "SAFE_MODE";
        case TAG_SYSTEM_ALERT: return "SYSTEM_ALERT";
        case TAG_SLEEP:        return "SLEEP";
        case TAG_TALK_LISTEN:  return "TALK_LISTEN";
        case TAG_TALK_THINK:   return "TALK_THINK";
        case TAG_MUSIC:        return "MUSIC";
        case TAG_ENGAGED_AWE:  return "ENGAGED_AWE";
        case TAG_ENGAGED_GLEE: return "ENGAGED_GLEE";
        case TAG_ENGAGED_HAPPY:return "ENGAGED_HAPPY";
        case TAG_ENGAGED_FOCUS:return "ENGAGED_FOCUS";
        case TAG_ENGAGED_SAD:  return "ENGAGED_SAD";
        case TAG_ENGAGED_NEUT: return "ENGAGED_NEUT";
        case TAG_IDLE_SLEEPY:  return "IDLE_SLEEPY";
        case TAG_IDLE_UNIMP:   return "IDLE_UNIMP";
        case TAG_IDLE_GLEE:    return "IDLE_GLEE";
        case TAG_IDLE_HAPPY:   return "IDLE_HAPPY";
        case TAG_IDLE_SAD_D:   return "IDLE_SAD_D";
        case TAG_IDLE_SAD_U:   return "IDLE_SAD_U";
        case TAG_IDLE_FOCUS:   return "IDLE_FOCUS";
        case TAG_IDLE_AWE:     return "IDLE_AWE";
        case TAG_IDLE_NEUTRAL: return "IDLE_NEUTRAL";
        default:               return "UNKNOWN";
    }
}

static bool face_params_need_recover(const face_params_t *p)
{
    return (p->open_l <= 0.24f) && (p->open_r <= 0.24f);
}

static void behavior_engine_process_pending_reactions(uint32_t now)
{
    reaction_category_t category;

    for (category = REACTION_CATEGORY_SAFETY;
         category < REACTION_CATEGORY_COUNT;
         category = (reaction_category_t)(category + 1)) {
        bool do_clear;
        bool do_submit;
        face_command_t cmd;

        taskENTER_CRITICAL(&s_reaction_req_lock);
        do_clear = s_pending_reaction_clear[category];
        do_submit = s_pending_reaction_submit[category];
        cmd = s_pending_reaction_cmds[category];
        s_pending_reaction_clear[category] = false;
        s_pending_reaction_submit[category] = false;
        taskEXIT_CRITICAL(&s_reaction_req_lock);

        if (do_clear) {
            reaction_resolver_clear(&s_reaction_resolver, category);
        }

        if (do_submit) {
            reaction_resolver_submit(&s_reaction_resolver, category, &cmd, now);
        }
    }
}

static void behavior_engine_apply_composed_face(uint32_t now)
{
    face_command_t final_cmd;
    face_params_t current_target;
    const bool has_active_overlays =
        reaction_resolver_has_active(&s_reaction_resolver, now);

    reaction_resolver_tick(&s_reaction_resolver, now);

    face_engine_get_target(&current_target);
    if (!has_active_overlays &&
        !tag_is_sleep_like(s_face_tag) &&
        face_params_need_recover(&current_target)) {
        ESP_LOGD(TAG,
                 "FACE_RECOVER stuck blink tag=%s target=(%.2f,%.2f,x=%u)",
                 face_tag_name(s_face_tag),
                 (double)current_target.open_l,
                 (double)current_target.open_r,
                 (unsigned)current_target.x_off);
        s_face_force_apply = true;
        s_baseline_dirty = true;
    }

    if (!s_baseline_dirty && !s_face_force_apply && !has_active_overlays) {
        return;
    }

    if (!reaction_resolver_compose(&s_reaction_resolver, &s_baseline_cmd,
                                   now, &final_cmd)) {
        return;
    }

    final_cmd.params = normalize_runtime_face(s_face_tag, final_cmd.params);
    face_engine_apply_params(&final_cmd.params);
    s_baseline_dirty = false;
    s_face_force_apply = false;
}

static void set_face(uint8_t tag, face_params_t p)
{
    const bool tag_changed = (tag != s_face_tag);
    p = normalize_baseline_face(tag, p);

    if (tag_changed || s_face_force_apply) {
        face_engine_set_alert_overlay(tag == TAG_SYSTEM_ALERT);
        face_engine_set_sleep_overlay(tag == TAG_SLEEP);

        if (tag_changed) {
            ESP_LOGD(TAG, "FACE -> %s", face_tag_name(tag));
        }

        s_baseline_cmd.params = p;
        s_baseline_cmd.issued_ms = now_ms();
        s_baseline_cmd.hold_ms = 0u;
        s_baseline_cmd.fade_ms = 0u;
        s_baseline_cmd.priority = FACE_CMD_PRIO_BASELINE;
        s_baseline_cmd.source = FACE_CMD_SRC_BASELINE;
        s_baseline_cmd.interruptible = true;
        s_baseline_cmd.additive_gaze = false;
        s_baseline_cmd.additive_eyelid = false;
        s_baseline_cmd.additive_brow = false;
        s_baseline_cmd.params.priority = FACE_CMD_PRIO_BASELINE;

        s_face_tag = tag;
        s_baseline_dirty = true;
    }
}

/* ── Callbacks de evento (executam na task do EventBus) ──────────────── */

static void on_touch(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    g_state.being_touched = true;
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
    ESP_LOGD(TAG, "LOWBAT — energy=%.2f", g_state.energy);
}

static void on_servo_blocked(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    s_safe_mode_req = true;
}

/* ── FSM tick ────────────────────────────────────────────────────────── */

static void fsm_tick(void)
{
    if (s_safe_mode_req) {
        s_safe_mode_req = false;
        s_safe_exit_us  = esp_timer_get_time() + SAFE_MODE_DURATION_US;
        if (s_state != BSTATE_SAFE_MODE)
            ESP_LOGD(TAG, "→ SAFE_MODE (servo bloqueado)");
        s_state = BSTATE_SAFE_MODE;
        return;
    }

    if (s_sleep_req) {
        s_sleep_req = false;
        if (s_state != BSTATE_SLEEP) {
            ESP_LOGD(TAG, "→ SLEEP (request)");
        }
        s_state = BSTATE_SLEEP;
        return;
    }

    if (s_wake_req) {
        s_wake_req = false;
        if (s_state != BSTATE_IDLE) {
            ESP_LOGD(TAG, "%s → IDLE (request)", behavior_state_name(s_state));
        }
        s_state = BSTATE_IDLE;
        return;
    }

    switch (s_state) {

        case BSTATE_SAFE_MODE:
            if (esp_timer_get_time() >= s_safe_exit_us) {
                ESP_LOGD(TAG, "SAFE_MODE → IDLE");
                s_state = BSTATE_IDLE;
            }
            break;

        case BSTATE_SLEEP:
            if (g_state.energy >= 0.15f || g_state.attention > 0.3f) {
                ESP_LOGD(TAG, "SLEEP → IDLE  energy=%.2f", g_state.energy);
                s_state = BSTATE_IDLE;
            }
            break;

        case BSTATE_IDLE:
            if (g_state.energy < 0.15f) {
                ESP_LOGD(TAG, "IDLE → SLEEP  energy=%.2f", g_state.energy);
                s_state = BSTATE_SLEEP;
            } else if (g_state.attention > 0.5f || g_state.person_present) {
                ESP_LOGD(TAG, "IDLE → ENGAGED  attn=%.2f", g_state.attention);
                s_state = BSTATE_ENGAGED;
            }
            break;

        case BSTATE_ENGAGED:
            if (s_wake_word_req) {
                s_wake_word_req = false;
                ESP_LOGD(TAG, "ENGAGED → TALKING");
                s_state = BSTATE_TALKING;
            } else if (g_state.attention < 0.3f && !g_state.person_present) {
                ESP_LOGD(TAG, "ENGAGED → IDLE  attn=%.2f", g_state.attention);
                s_state = BSTATE_IDLE;
            }
            break;

        case BSTATE_TALKING:
            if (g_state.attention < TALKING_ATTN_EXIT) {
                ESP_LOGD(TAG, "TALKING → ENGAGED  attn=%.2f", g_state.attention);
                s_state = BSTATE_ENGAGED;
            }
            break;

        default:
            s_state = BSTATE_IDLE;
            break;
    }
}

/* ── Nós folha da behavior tree ──────────────────────────────────────── */

/* P1 — Segurança crítica */
static bt_status_t node_safe_mode(void)
{
    if (s_state != BSTATE_SAFE_MODE) return BT_FAILURE;
    face_params_t p = FACE_SCARED;
    p.transition_ms = 150;
    set_face(TAG_SAFE_MODE, p);
    return BT_SUCCESS;
}

/* P2 — Alerta de sistema: bateria crítica (comfort < 0.10) */
static bt_status_t node_system_alert(void)
{
    if (g_state.comfort >= 0.10f) return BT_FAILURE;
    face_params_t p = make_tuned_neutral(FACE_WORRIED, 0.96f, 108, 280);
    p.cv_top = 2;
    p.cv_bot = -2;
    p.y_l = 2;
    p.y_r = 2;
    set_face(TAG_SYSTEM_ALERT, p);
    return BT_SUCCESS;
}

/* P3 — Em sono */
static bt_status_t node_sleep(void)
{
    if (s_state != BSTATE_SLEEP) return BT_FAILURE;
    face_params_t p = FACE_SLEEP;
    p.transition_ms = 800;
    set_face(TAG_SLEEP, p);
    return BT_SUCCESS;
}

/* P5 — Diálogo ativo: diferencia LISTENING (atenção alta) de THINKING */
static bt_status_t node_talking(void)
{
    if (s_state != BSTATE_TALKING) return BT_FAILURE;

    /* Atenção ainda alta → ouvindo (FOCUSED). Decaindo → processando com um
     * "think" mais legível que um squint puro em runtime. */
    if (g_state.attention >= 0.90f) {
        face_params_t p =
            make_tuned_talk_focus(FACE_FOCUSED, 0.66f, 116, 1, -6, 160);
        set_face(TAG_TALK_LISTEN, p);
    } else {
        face_params_t p =
            make_tuned_talk_focus(FACE_FOCUSED, 0.58f, 114, 2, -6, 260);
        set_face(TAG_TALK_THINK, p);
    }
    return BT_SUCCESS;
}

/* P7 — Música detectada */
static bt_status_t node_music(void)
{
    if (!g_state.music_detected) return BT_FAILURE;
    face_params_t p = make_tuned_happy(FACE_HAPPY, 0.50f, 116, 320);
    set_face(TAG_MUSIC, p);
    return BT_SUCCESS;
}

/* P8 — ENGAGED: seleciona preset por StateVector
 * Social & AttentionSystem doc 9.1: arousal alto + pessoa → AWE/CURIOUS
 * AnimationLibrary doc 2.2: valence > 0.5 → VERY_HAPPY (GLEE)          */
static bt_status_t node_engaged(void)
{
    if (s_state != BSTATE_ENGAGED) return BT_FAILURE;

    uint8_t       tag;
    face_params_t p;
    persona_state_t persona;

    persona_service_get_snapshot(&persona);

    if (g_state.mood_arousal > (persona.expressiveness > 0.60f ? 0.50f : 0.60f)) {
        /* Muito excitado com presença → maravilhado (CURIOUS per doc 9.1) */
        tag = TAG_ENGAGED_AWE;
        p = FACE_AWE;          p.transition_ms = 240;
        p.open_l = 0.96f;
        p.open_r = 0.96f;
        p.x_off = 106;
    } else if (g_state.mood_valence > (persona.bond_bias > 0.55f ? 0.40f : 0.50f)) {
        /* Muito feliz com a pessoa → GLEE (VERY_HAPPY) */
        tag = TAG_ENGAGED_GLEE;
        p = make_tuned_happy(FACE_GLEE, 0.40f, 116, 300);
    } else if (g_state.mood_valence > 0.25f ||
               (persona.bond_bias > 0.60f && g_state.mood_valence > 0.10f)) {
        tag = TAG_ENGAGED_HAPPY;
        p = make_tuned_happy(FACE_HAPPY, 0.48f, 116, 280);
    } else if (persona.reserve > 0.60f && g_state.mood_valence < 0.10f) {
        tag = TAG_ENGAGED_FOCUS;
        p = make_tuned_focus(FACE_FOCUSED, 0.64f, 114, -7, 280);
    } else if (g_state.attention > 0.80f) {
        tag = TAG_ENGAGED_FOCUS;
        p = make_tuned_focus(FACE_FOCUSED, 0.64f, 114, -7, 260);
    } else if (g_state.mood_valence < -0.25f) {
        tag = TAG_ENGAGED_SAD;
        p = make_tuned_neutral(FACE_SAD_UP, 0.82f, 112, 500);
    } else {
        tag = TAG_ENGAGED_NEUT;
        p = make_tuned_neutral(FACE_NEUTRAL, 0.95f, 112, 320);
    }

    set_face(tag, p);
    return BT_SUCCESS;
}

/* P10 — Idle normal: seleciona preset por StateVector (sempre SUCCESS)
 *
 * Mapeamento conforme docs:
 *   energy < 0.20          → SLEEPY     (BehavioralStateMachine 3.1: LIGHT_SLEEP)
 *   energy < 0.35          → UNIMP      (TIRED expression)
 *   valence > 0.60         → GLEE       (AnimationLibrary: VERY_HAPPY)
 *   valence > 0.35         → HAPPY
 *   valence < -0.25        → SAD_DOWN
 *   valence < -0.10        → SAD_UP     (leve tristeza)
 *   attention > 0.70       → FOCUSED    (squint leve de atenção)
 *   social_need > 0.60     → AWE        (Social&Attention 9.4: procurando pessoa)
 *   default                → NEUTRAL
 */
static bt_status_t node_idle(void)
{
    uint8_t       tag;
    face_params_t p;
    persona_state_t persona;

    persona_service_get_snapshot(&persona);

    if (g_state.energy < 0.20f) {
        tag = TAG_IDLE_SLEEPY;  p = FACE_SLEEPY;   p.transition_ms = 760;
        p.open_l = 0.52f;
        p.open_r = 0.70f;
        p.x_off = 110;
    } else if (g_state.energy < 0.35f) {
        tag = TAG_IDLE_UNIMP;   p = FACE_UNIMP;    p.transition_ms = 560;
        p.open_l = 0.54f;
        p.open_r = 0.58f;
        p.x_off = 114;
    } else if (g_state.mood_valence > (persona.expressiveness > 0.60f ? 0.50f : 0.60f)) {
        tag = TAG_IDLE_GLEE;    p = make_tuned_happy(FACE_GLEE, 0.40f, 116, 400);
    } else if (g_state.mood_valence > 0.35f ||
               (persona.bond_bias > 0.60f && g_state.mood_valence > 0.20f)) {
        tag = TAG_IDLE_HAPPY;   p = make_tuned_happy(FACE_HAPPY, 0.48f, 116, 380);
    } else if (g_state.mood_valence < -0.25f) {
        tag = TAG_IDLE_SAD_D;   p = make_tuned_neutral(FACE_SAD_DOWN, 0.74f, 112, 600);
    } else if (g_state.mood_valence < -0.10f) {
        tag = TAG_IDLE_SAD_U;   p = make_tuned_neutral(FACE_SAD_UP, 0.82f, 112, 600);
    } else if (persona.reserve > 0.60f && g_state.attention > 0.55f) {
        tag = TAG_IDLE_FOCUS;   p = make_tuned_focus(FACE_FOCUSED, 0.64f, 114, -7, 320);
    } else if (g_state.attention > 0.70f) {
        tag = TAG_IDLE_FOCUS;   p = make_tuned_focus(FACE_FOCUSED, 0.64f, 114, -7, 320);
    } else if (g_state.social_need > (persona.social_drive > 0.55f ? 0.45f : 0.60f)) {
        tag = TAG_IDLE_AWE;     p = FACE_AWE;      p.transition_ms = 500;
        p.open_l = 0.96f;
        p.open_r = 0.96f;
        p.x_off = 106;
    } else {
        tag = TAG_IDLE_NEUTRAL; p = make_tuned_neutral(FACE_NEUTRAL, 0.97f, 112, 400);
    }

    set_face(tag, p);
    return BT_SUCCESS;
}

/* ── Raiz da árvore (selector — primeira SUCCESS vence) ─────────────── */

static const bt_node_fn k_root_nodes[] = {
    node_safe_mode,      /* P1 — segurança crítica               */
    node_system_alert,   /* P2 — bateria crítica / comfort baixo  */
    node_sleep,          /* P3 — em sono                         */
    node_talking,        /* P5 — wake word / diálogo ativo       */
    node_music,          /* P7 — música detectada                */
    node_engaged,        /* P8 — pessoa presente / atenção alta  */
    node_idle,           /* P10 — fallback por StateVector        */
};
#define ROOT_COUNT (sizeof(k_root_nodes) / sizeof(k_root_nodes[0]))

/* ── BehaviorLoopTask ────────────────────────────────────────────────── */

static void behavior_loop_task(void *arg)
{
    (void)arg;

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        const uint32_t now = now_ms();

        /* Ajusta taxa de blink pela energia atual */
        blink_set_energy(g_state.energy);
        persona_service_tick(now);
        gesture_service_tick(now);

        fsm_tick();
        bt_selector(k_root_nodes, ROOT_COUNT);
        behavior_engine_process_pending_reactions(now);
        behavior_engine_apply_composed_face(now);

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(100));
    }
}

/* ── API pública ─────────────────────────────────────────────────────── */

void behavior_engine_init(void)
{
    reaction_resolver_init(&s_reaction_resolver);
    persona_service_init();
    gesture_service_init();

    event_bus_subscribe(EVT_TOUCH_DETECTED, on_touch);
    event_bus_subscribe(EVT_WAKE_WORD,      on_wake_word);
    event_bus_subscribe(EVT_SYS_LOWBAT,    on_lowbat);
    event_bus_subscribe(EVT_SERVO_BLOCKED,  on_servo_blocked);

    xTaskCreatePinnedToCore(behavior_loop_task, "BehaviorLoop",
                            4096, NULL, 12, NULL, 1);

    ESP_LOGD(TAG, "init  BehaviorLoopTask Core 1 pri 12 tick=100ms");
}

behavior_state_t behavior_engine_get_state(void)
{
    return s_state;
}

void behavior_engine_refresh_face(void)
{
    s_face_force_apply = true;
}

void behavior_engine_request_sleep(void)
{
    s_wake_req = false;
    s_sleep_req = true;
}

void behavior_engine_request_wake(void)
{
    s_sleep_req = false;
    s_wake_req = true;
}

bool behavior_engine_submit_reaction(reaction_category_t category,
                                     const face_command_t *cmd)
{
    if (cmd == NULL || category >= REACTION_CATEGORY_COUNT) {
        return false;
    }

    taskENTER_CRITICAL(&s_reaction_req_lock);
    s_pending_reaction_cmds[category] = *cmd;
    s_pending_reaction_submit[category] = true;
    s_pending_reaction_clear[category] = false;
    taskEXIT_CRITICAL(&s_reaction_req_lock);
    return true;
}

void behavior_engine_clear_reaction(reaction_category_t category)
{
    if (category >= REACTION_CATEGORY_COUNT) {
        return;
    }

    taskENTER_CRITICAL(&s_reaction_req_lock);
    s_pending_reaction_clear[category] = true;
    s_pending_reaction_submit[category] = false;
    taskEXIT_CRITICAL(&s_reaction_req_lock);
}

const char *behavior_state_name(behavior_state_t s)
{
    static const char *names[BSTATE_COUNT] = {
        "SLEEP", "IDLE", "ENGAGED", "TALKING", "SAFE_MODE",
    };
    return (s < BSTATE_COUNT) ? names[s] : "UNKNOWN";
}

