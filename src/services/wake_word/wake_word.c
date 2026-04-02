#include "wake_word.h"
#include "event_bus.h"
#include "emotion_mapper.h"

/* Inclui apenas esp-sr — NÃO inclui audio_capture.h pois vad.h colide
 * com esp_vad.h do esp-sr (ambos declaram vad_process com assinaturas distintas).
 * audio_capture_read é declarada aqui como extern para evitar o conflito. */
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "model_path.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include <stdint.h>
#include <stddef.h>

/* Forward-declare audio_capture_read sem incluir audio_capture.h
 * (que puxa vad.h e causaria colisão com esp_vad.h do esp-sr). */
extern size_t audio_capture_read(int16_t *buf, size_t samples);

static const char *TAG = "wake_word";

#define TASK_STACK        8192u
#define TASK_PRIO           15u
#define SUPPRESS_AFTER_MS  800u   /* auto-supressão após detecção */

static const esp_afe_sr_iface_t *s_afe_iface        = NULL;
static esp_afe_sr_data_t        *s_afe_data          = NULL;
static volatile uint32_t         s_suppress_until_ms = 0u;

/* ── Helpers ───────────────────────────────────────────────────────────── */

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── API pública ───────────────────────────────────────────────────────── */

void wake_word_suppress_ms(uint32_t ms)
{
    s_suppress_until_ms = now_ms() + ms;
}

/* ── Task (Core 0) ─────────────────────────────────────────────────────── */

static void wake_word_task(void *arg)
{
    (void)arg;

    int      feed_size = s_afe_iface->get_feed_chunksize(s_afe_data);
    int16_t *feed_buf  = heap_caps_malloc((size_t)feed_size * sizeof(int16_t),
                                           MALLOC_CAP_SPIRAM);
    if (!feed_buf) {
        ESP_LOGE(TAG, "sem PSRAM para feed_buf (%d samples)", feed_size);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "task rodando — feed_size=%d samples", feed_size);

    int accum = 0;

    while (1) {
        /* Acumula samples até ter feed_size para o AFE.
         * audio_capture_read devolve até 512 amostras por chamada. */
        while (accum < feed_size) {
            size_t got = audio_capture_read(feed_buf + accum,
                                            (size_t)(feed_size - accum));
            if (got == 0) continue;
            accum += (int)got;
        }
        accum = 0;

        /* Feed do bloco de áudio para a pipeline AFE */
        if (s_afe_iface->feed(s_afe_data, feed_buf) < 0) continue;

        /* Fetch resultado (bloqueante, timeout interno de 2000ms) */
        afe_fetch_result_t *res = s_afe_iface->fetch(s_afe_data);
        if (!res) continue;

        if (res->wakeup_state != WAKENET_DETECTED) continue;

        uint32_t t = now_ms();

        /* Verifica supressão (auto-supressão pós-detecção ou playback) */
        if (t < s_suppress_until_ms) {
            ESP_LOGD(TAG, "suprimida (%"PRIu32"ms restantes)",
                     s_suppress_until_ms - t);
            continue;
        }

        ESP_LOGI(TAG, "WAKE WORD! ww_index=%d", res->wake_word_index);

        /* Auto-supressão: evita re-trigger durante WHOOSH (~200ms) */
        wake_word_suppress_ms(SUPPRESS_AFTER_MS);

        /* EVT_WAKE_WORD — audio_feedback.c toca WHOOSH; led_router seta LED_STATE_LISTENING */
        event_bus_publish(EVT_WAKE_WORD, NULL, 0u, EVENT_PRIO_BEHAVIOR);

        /* Reação facial */
        emotion_mapper_apply(EMOTION_SURPRISED, 200u);
    }
}

/* ── Init ─��────────────────────────────────────────────────────────────── */

esp_err_t wake_word_init(void)
{
    /* Carrega modelos da partição SPIFFS label "model" */
    srmodel_list_t *models = esp_srmodel_init("model");
    if (!models) {
        ESP_LOGE(TAG, "falha ao carregar modelos (partição 'model' montada?)");
        return ESP_FAIL;
    }

    /* Seleciona a WakeNet configurada via menuconfig (e.g. Jarvis) */
    char *wn_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);
    if (!wn_name) {
        ESP_LOGE(TAG, "nenhuma WakeNet disponível nos modelos");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "modelo WakeNet: %s", wn_name);

    /* Cria configuração AFE: mono mic ("M"), sem refer��ncia de speaker,
     * modo low-cost para economizar CPU no Core 0. */
    afe_config_t *cfg = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (!cfg) {
        ESP_LOGE(TAG, "falha ao criar afe_config");
        return ESP_FAIL;
    }
    cfg->wakenet_init       = true;
    cfg->wakenet_model_name = wn_name;
    cfg->aec_init           = false;
    cfg->memory_alloc_mode  = AFE_MEMORY_ALLOC_MORE_PSRAM;

    /* Obtém iface correspondente ao config e cria dados AFE */
    s_afe_iface = esp_afe_handle_from_config(cfg);
    if (!s_afe_iface) {
        ESP_LOGE(TAG, "esp_afe_handle_from_config falhou");
        afe_config_free(cfg);
        return ESP_FAIL;
    }

    s_afe_data = s_afe_iface->create_from_config(cfg);
    afe_config_free(cfg);

    if (!s_afe_data) {
        ESP_LOGE(TAG, "create_from_config falhou (PSRAM suficiente?)");
        return ESP_FAIL;
    }

    s_suppress_until_ms = 0u;

    BaseType_t rc = xTaskCreatePinnedToCore(
        wake_word_task, "wake_word",
        TASK_STACK, NULL, TASK_PRIO, NULL, 0 /* Core 0 */);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "falha ao criar task (sem memória)");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ok — P%u Core0 suppress=%ums", TASK_PRIO, SUPPRESS_AFTER_MS);
    return ESP_OK;
}
