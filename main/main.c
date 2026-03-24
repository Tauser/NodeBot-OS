#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <inttypes.h>

#include "boot_sequence.h"
#include "face_engine.h"

static const char *TAG = "test_face";

/* ════════════════════════════════════════════════════════════════════════ */

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "  Face Engine — teste E16");
    ESP_LOGI(TAG, "════════════════════════════════════════");

    /* Inicializa todos os subsistemas (display, face_engine, etc.) */
    app_boot();

    /* ── Fase 1: expressão neutra (3 s) ──────────────────────────────── */
    ESP_LOGI(TAG, "[1] neutro  eye=80 mouth=50");
    face_params_t p = { .eye_open = 80, .mouth_curve = 50, .blink = 0 };
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 2: sorriso (3 s) ───────────────────────────────────────── */
    ESP_LOGI(TAG, "[2] feliz   eye=90 mouth=90");
    p.eye_open    = 90;
    p.mouth_curve = 90;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 3: triste (3 s) ────────────────────────────────────────── */
    ESP_LOGI(TAG, "[3] triste  eye=40 mouth=10");
    p.eye_open    = 40;
    p.mouth_curve = 10;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 4: piscada (1 s) ───────────────────────────────────────── */
    ESP_LOGI(TAG, "[4] piscada");
    p.eye_open    = 80;
    p.mouth_curve = 50;
    p.blink       = 1;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(150));
    p.blink = 0;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(850));

    /* ── Fase 5: loop — face sorridente fixa ──────────────────────────── */
    ESP_LOGI(TAG, "[5] loop — face sorridente fixa. FPS log a cada 5 s.");
    p.eye_open    = 85;
    p.mouth_curve = 75;
    p.blink       = 0;
    face_engine_apply_params(&p);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
