#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "boot_sequence.h"
#include "face_engine.h"

static const char *TAG = "test_face";

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "  Face Engine — teste EMO/EILIK");
    ESP_LOGI(TAG, "════════════════════════════════════════");

    app_boot();

    /* ── Fase 1: neutro (3 s) ─────────────────────────────────────── */
    ESP_LOGI(TAG, "[1] NEUTRAL");
    face_params_t p = FACE_NEUTRAL;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 2: feliz (3 s) ──────────────────────────────────────── */
    ESP_LOGI(TAG, "[2] HAPPY");
    p = FACE_HAPPY;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 3: triste (3 s) ─────────────────────────────────────── */
    ESP_LOGI(TAG, "[3] SAD");
    p = FACE_SAD;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 4: raiva (3 s) ──────────────────────────────────────── */
    ESP_LOGI(TAG, "[4] ANGRY");
    p = FACE_ANGRY;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 5: surpreso (3 s) ───────────────────────────────────── */
    ESP_LOGI(TAG, "[5] SURPRISED");
    p = FACE_SURPRISED;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Fase 6: cansado (3 s) ────────────────────────────────────── */
    ESP_LOGI(TAG, "[6] TIRED");
    p = FACE_TIRED;
    face_engine_apply_params(&p);
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Loop: neutro fixo — FPS log a cada 5 s ───────────────────── */
    ESP_LOGI(TAG, "[loop] NEUTRAL — FPS log a cada 5 s");
    p = FACE_NEUTRAL;
    face_engine_apply_params(&p);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
