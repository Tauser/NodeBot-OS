#include "blink_controller.h"
#include "face_engine.h"
#include "face_params.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "BLINK";

/* ── Estado interno ─────────────────────────────────────────────────────── */
static volatile float s_energy   = 0.5f;   /* 0.0–1.0                      */
static volatile bool  s_suppress = false;   /* true = inibe próximo blink   */

/* ── Constantes de timing ───────────────────────────────────────────────── */
static constexpr uint32_t BLINK_CLOSE_MS  =  80u;  /* duração do fechar     */
static constexpr uint32_t BLINK_OPEN_MS   = 120u;  /* duração do abrir      */
static constexpr uint32_t BLINK_MARGIN_MS =  40u;  /* margem pós-animação   */

/* ── blink_task ─────────────────────────────────────────────────────────── */
static void blink_task(void *arg)
{
    (void)arg;

    for (;;) {
        /* Intervalo até próximo blink:
         *   energy=0 → ~5000 ms (sonolento)
         *   energy=1 → ~2500 ms (alerta)
         *   ± jitter de ±1000 ms */
        const float  e        = s_energy;
        const int    base_ms  = 5000 - (int)(e * 2500.0f);
        const int    jitter   = (int)(esp_random() % 2001u) - 1000;
        int          delay_ms = base_ms + jitter;
        if (delay_ms < 800)  delay_ms = 800;
        if (delay_ms > 7000) delay_ms = 7000;

        vTaskDelay(pdMS_TO_TICKS((uint32_t)delay_ms));

        /* Verifica supressão */
        if (s_suppress) {
            ESP_LOGD(TAG, "blink suprimido");
            continue;
        }

        /* Salva expressão-alvo atual */
        face_params_t saved;
        face_engine_get_target(&saved);

        /* ── Fecha — pálpebra desce (BLINK_HIGH: bl/br fecham, y sobe) ── */
        face_params_t blink_close = saved;
        blink_close.open_l        = 0.05f;
        blink_close.open_r        = 0.05f;
        blink_close.bl_l          = 28;
        blink_close.br_l          = 28;
        blink_close.bl_r          = 28;
        blink_close.br_r          = 28;
        blink_close.y_l           = -18;
        blink_close.y_r           = -18;
        blink_close.transition_ms = (uint16_t)BLINK_CLOSE_MS;
        face_engine_apply_params(&blink_close);

        vTaskDelay(pdMS_TO_TICKS(BLINK_CLOSE_MS + BLINK_MARGIN_MS));

        /* ── Reabre ───────────────────────────────────────────────────── */
        saved.transition_ms = (uint16_t)BLINK_OPEN_MS;
        face_engine_apply_params(&saved);

        vTaskDelay(pdMS_TO_TICKS(BLINK_OPEN_MS + BLINK_MARGIN_MS));

        ESP_LOGD(TAG, "blink ok  next=%d ms", delay_ms);
    }
}

/* ── API pública ────────────────────────────────────────────────────────── */

void blink_controller_init(void)
{
    xTaskCreatePinnedToCore(
        blink_task,
        "BlinkTask",
        2048,
        nullptr,
        5,          /* prioridade abaixo do render task (20) e da app */
        nullptr,
        0           /* Core 0 — render task fica no Core 1            */
    );
    ESP_LOGI(TAG, "BlinkTask iniciada (Core 0, pri 5)");
}

void blink_set_energy(float energy)
{
    if (energy < 0.0f) energy = 0.0f;
    if (energy > 1.0f) energy = 1.0f;
    s_energy = energy;
}

void blink_suppress(bool suppress)
{
    s_suppress = suppress;
}
