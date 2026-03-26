#include "wake_word.h"
#include "event_bus.h"

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"
#include "model_path.h"
#include "esp_wn_iface.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "WAKE";

/* ── Estado ────────────────────────────────────────────────────────── */
static srmodel_list_t              *s_models    = NULL;
static const esp_afe_sr_iface_t    *s_afe_iface = NULL;
static esp_afe_sr_data_t           *s_afe_data  = NULL;
static int                          s_feed_chunk = 0;
static volatile int64_t             s_suppress_until = 0;
static bool                         s_ready = false;

/* Acumulador para chunk do AFE (pode ser diferente de 256) */
#define FEED_BUF_MAX 1024
static int16_t s_accum[FEED_BUF_MAX];
static int     s_accum_pos = 0;

/* ── Stub de privacidade ────────────────────────────────────────────── */
__attribute__((weak)) bool privacy_policy_is_active(void) { return false; }

/* ── Task de fetch — consome resultados do AFE ─────────────────────── */
static void afe_fetch_task(void *arg)
{
    (void)arg;

    for (;;) {
        afe_fetch_result_t *res = s_afe_iface->fetch(s_afe_data);
        if (!res) continue;

        if (res->wakeup_state != WAKENET_DETECTED) continue;

        if (esp_timer_get_time() < s_suppress_until) continue;
        if (privacy_policy_is_active()) continue;

        wake_word_event_t evt = {
            .confidence   = 1.0f,
            .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000LL),
            .word_index   = (uint8_t)res->wake_word_index,
        };
        event_bus_publish(EVT_WAKE_WORD, &evt, sizeof(evt), EVENT_PRIO_SYSTEM);

        ESP_LOGI(TAG, "WAKE WORD  idx=%d  ts=%ums",
                 (int)res->wake_word_index, (unsigned)evt.timestamp_ms);
    }
}

/* ── Init ──────────────────────────────────────────────────────────── */

bool wake_word_init(const void *model_data, size_t model_size)
{
    (void)model_data;
    (void)model_size;

    /* 1. Carrega modelos da partição flash */
    s_models = esp_srmodel_init("model");
    if (!s_models) {
        ESP_LOGE(TAG, "esp_srmodel_init falhou");
        return false;
    }

    /* 2. Cria config AFE — "M" = mono mic, sem canal de referência */
    afe_config_t *cfg = afe_config_init("M", s_models,
                                        AFE_TYPE_SR,
                                        AFE_MODE_LOW_COST);
    if (!cfg) {
        ESP_LOGE(TAG, "afe_config_init falhou");
        return false;
    }

    /* 3. Ajusta config para ESP32-S3 mono sem AEC */
    cfg->aec_init              = false;                      /* sem referência de playback */
    cfg->wakenet_mode          = DET_MODE_95;               /* modo agressivo              */
    cfg->memory_alloc_mode     = AFE_MEMORY_ALLOC_MORE_PSRAM;
    cfg->afe_perferred_core    = 0;                          /* Core 0, junto com AudioCap  */
    cfg->afe_perferred_priority = 13;

    afe_config_check(cfg);

    /* 4. Instancia AFE (cria tasks internas) */
    s_afe_iface = esp_afe_handle_from_config(cfg);
    if (!s_afe_iface) {
        ESP_LOGE(TAG, "esp_afe_handle_from_config falhou");
        free(cfg);
        return false;
    }

    s_afe_data = s_afe_iface->create_from_config(cfg);
    free(cfg);
    if (!s_afe_data) {
        ESP_LOGE(TAG, "AFE create_from_config falhou — PSRAM insuficiente?");
        return false;
    }

    s_feed_chunk = s_afe_iface->get_feed_chunksize(s_afe_data);
    if (s_feed_chunk <= 0 || s_feed_chunk > FEED_BUF_MAX) {
        ESP_LOGE(TAG, "feed_chunk=%d inválido", s_feed_chunk);
        s_afe_iface->destroy(s_afe_data);
        return false;
    }

    /* 5. Task que consome resultados do AFE */
    xTaskCreatePinnedToCore(afe_fetch_task, "AFEFetch",
                            4096, NULL, 13, NULL, 0);

    s_ready = true;
    ESP_LOGI(TAG, "AFE+WakeNet OK  feed_chunk=%d", s_feed_chunk);
    return true;
}

/* ── Feed — chamado pela AudioCaptureTask ──────────────────────────── */

float wake_word_feed(const int16_t *block, size_t samples)
{
    if (!s_ready || !block || samples == 0) return 0.0f;

    size_t in = 0;
    while (in < samples) {
        int space = s_feed_chunk - s_accum_pos;
        int copy  = ((int)(samples - in) < space) ? (int)(samples - in) : space;
        memcpy(&s_accum[s_accum_pos], &block[in], (size_t)copy * sizeof(int16_t));
        s_accum_pos += copy;
        in += (size_t)copy;

        if (s_accum_pos < s_feed_chunk) break;
        s_accum_pos = 0;

        s_afe_iface->feed(s_afe_data, s_accum);
    }

    return 0.0f;  /* resultado vem assincronamente via afe_fetch_task */
}

/* ── Supressão e status ────────────────────────────────────────────── */

void wake_word_suppress_ms(uint32_t ms)
{
    s_suppress_until = esp_timer_get_time() + (int64_t)ms * 1000LL;
}

bool wake_word_is_ready(void) { return s_ready; }
