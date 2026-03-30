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

/* ── Constantes ──────────────────────────────────────────────────────── */
#define YAWN_COOLDOWN_MS      (8u * 60u * 1000u)   /* 8 min entre bocejos  */
#define YAWN_ENERGY_THR       0.4f                  /* energy abaixo disso  */
#define YAWN_IDLE_THR_MS      300000u               /* 5 min sem interação  */
#define YAWN_PROB_PCT         40u                   /* 40 % de chance       */
#define YAWN_CHECK_INTERVAL   60000u                /* verifica a cada 60 s */
#define IDLE_VAR_MIN_MS       20000u
#define IDLE_VAR_RANGE_MS     20001u                /* [20 s, 40 s)         */

static volatile uint32_t s_last_yawn_ms = 0u;

/* ── Helpers ─────────────────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int8_t clamp8(int v)
{
    return v > 127 ? (int8_t)127 : (v < -128 ? (int8_t)-128 : (int8_t)v);
}

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── Bocejo ──────────────────────────────────────────────────────────── */

void idle_behavior_trigger_yawn(void)
{
    const uint32_t now = now_ms();

    if (s_last_yawn_ms != 0u && (now - s_last_yawn_ms) < YAWN_COOLDOWN_MS) {
        ESP_LOGD(TAG, "yawn cooldown ativo (%.1f min restantes)",
                 (float)(YAWN_COOLDOWN_MS - (now - s_last_yawn_ms)) / 60000.0f);
        return;
    }
    s_last_yawn_ms = now;
    ESP_LOGI(TAG, "bocejo");

    /* Salva expressão atual e suprime blinks durante a animação */
    face_params_t base;
    face_engine_get_target(&base);
    blink_suppress(true);

    /* ── KF1 (2 s): "boca abrindo"
     *   Cantos inferiores descem, olhos baixam levemente e cv_bot cresce,
     *   simulando o "esticar" característico do bocejo. */
    face_params_t kf = base;
    kf.bl_l          = clamp8((int)base.bl_l + 12);
    kf.br_l          = clamp8((int)base.br_l + 12);
    kf.bl_r          = clamp8((int)base.bl_r + 12);
    kf.br_r          = clamp8((int)base.br_r + 12);
    kf.y_l           = clamp8((int)base.y_l  +  6);
    kf.y_r           = clamp8((int)base.y_r  +  6);
    kf.cv_bot        = clamp8((int)base.cv_bot + 14);
    kf.transition_ms = 2000;
    face_engine_apply_params(&kf);
    vTaskDelay(pdMS_TO_TICKS(2100));

    /* ── KF2 (1 s): pálpebra cai para 70 % do valor base */
    kf.open_l        = clampf(base.open_l * 0.70f, 0.05f, 1.0f);
    kf.open_r        = clampf(base.open_r * 0.70f, 0.05f, 1.0f);
    kf.transition_ms = 1000;
    face_engine_apply_params(&kf);
    vTaskDelay(pdMS_TO_TICKS(1100));

    /* ── KF3 (1 s): hold */
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ── KF4 (2 s): revertendo */
    face_params_t restore = base;
    restore.transition_ms = 2000;
    face_engine_apply_params(&restore);
    vTaskDelay(pdMS_TO_TICKS(2100));

    blink_suppress(false);
    ESP_LOGD(TAG, "bocejo concluído");
}

/* ── Variações ociosas ───────────────────────────────────────────────── */

/* (a) Sorriso sutil: cv_bot negativo curva a parte inferior para cima */
static void variation_slight_smile(void)
{
    face_params_t base;
    face_engine_get_target(&base);

    face_params_t smile   = base;
    smile.cv_bot          = clamp8((int)base.cv_bot - 9);
    smile.open_l          = clampf(base.open_l * 0.92f, 0.05f, 1.0f);
    smile.open_r          = clampf(base.open_r * 0.92f, 0.05f, 1.0f);
    smile.transition_ms   = 700;
    face_engine_apply_params(&smile);

    vTaskDelay(pdMS_TO_TICKS(3000));

    base.transition_ms = 700;
    face_engine_apply_params(&base);
    vTaskDelay(pdMS_TO_TICKS(800));   /* aguarda transição concluir */
}

/* (b) Pensativo: olhar ligeiramente para cima por 3 s */
static void variation_pensive(void)
{
    /* gaze y=-0.2 → levemente para cima (y negativo = cima) */
    gaze_service_set_target(0.0f, -0.2f, 250);
    vTaskDelay(pdMS_TO_TICKS(3000));
    gaze_service_set_target(0.0f,  0.0f, 250);
    vTaskDelay(pdMS_TO_TICKS(350));
}

/* (c) Piscar rápido duplo */
static void variation_double_blink(void)
{
    blink_controller_trigger();
    vTaskDelay(pdMS_TO_TICKS(380));
    blink_controller_trigger();
}

static void do_idle_variation(void)
{
    switch (esp_random() % 3u) {
    case 0:
        ESP_LOGD(TAG, "var: slight_smile");
        variation_slight_smile();
        break;
    case 1:
        ESP_LOGD(TAG, "var: pensive");
        variation_pensive();
        break;
    default:
        ESP_LOGD(TAG, "var: double_blink");
        variation_double_blink();
        break;
    }
}

/* ── IdleTask ────────────────────────────────────────────────────────── */

static void idle_task(void *arg)
{
    (void)arg;

    /* Primeira verificação de bocejo após 60 s do boot */
    uint32_t yawn_check_at = now_ms() + YAWN_CHECK_INTERVAL;

    for (;;) {
        /* Aguarda intervalo aleatório [20 s, 40 s) */
        const uint32_t wait_ms = IDLE_VAR_MIN_MS +
                                 (esp_random() % IDLE_VAR_RANGE_MS);
        vTaskDelay(pdMS_TO_TICKS(wait_ms));

        const uint32_t now = now_ms();

        /* ── Verificação periódica de bocejo (a cada 60 s) ─────────── */
        if (now >= yawn_check_at) {
            yawn_check_at = now + YAWN_CHECK_INTERVAL;

            const uint32_t idle_ms = now - g_state.last_interaction_ms;
            if (g_state.energy      <  YAWN_ENERGY_THR  &&
                idle_ms             >  YAWN_IDLE_THR_MS  &&
                (esp_random() % 100u) < YAWN_PROB_PCT) {

                idle_behavior_trigger_yawn();
                continue;   /* pula variação neste ciclo */
            }
        }

        /* ── Variação de vida ociosa ─────────────────────────────────── */
        do_idle_variation();
    }
}

/* ── Init ────────────────────────────────────────────────────────────── */

void idle_behavior_init(void)
{
    xTaskCreatePinnedToCore(idle_task, "IdleTask",
                            3072, NULL, 3, NULL, 0);
    ESP_LOGI(TAG, "IdleTask Core 0 pri 3  var=[20-40 s]  yawn_check=60 s");
}
