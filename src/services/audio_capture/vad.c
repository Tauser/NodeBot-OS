#include "vad.h"
#include "config_manager.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <math.h>
#include <stdint.h>

static const char *TAG = "vad";

/* ── Configuração ──────────────────────────────────────────────────────── */
#define ZCR_MIN          0.05f   /* ZCR mínima para fala (filtra ruído DC)  */
#define ZCR_MAX          0.45f   /* ZCR máxima para fala (filtra chiado)    */
#define NVS_KEY_RMS_THR  "vad_rms_thr"
#define RMS_THR_DEFAULT  200

/* ── Estado ────────────────────────────────────────────────────────────── */
static int32_t           s_rms_threshold;
static volatile uint32_t s_suppress_until_ms; /* uint32 → escrita atômica  */

/* ── Helpers ───────────────────────────────────────────────────────────── */
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── API ───────────────────────────────────────────────────────────────── */

void vad_init(void)
{
    s_rms_threshold    = config_get_int(NVS_KEY_RMS_THR, RMS_THR_DEFAULT);
    s_suppress_until_ms = 0u;
    ESP_LOGI(TAG, "rms_threshold=%"PRId32, s_rms_threshold);
}

vad_event_t vad_process(const int16_t *samples, size_t count)
{
    vad_event_t result = { .energy_db = -96.0f, .is_speech = false };

    if (count == 0) return result;

    /* Gate de supressão (playback ativo) */
    if (s_suppress_until_ms > 0u && now_ms() < s_suppress_until_ms) {
        return result;
    }

    /* RMS */
    int64_t sum_sq = 0;
    for (size_t i = 0; i < count; i++) {
        int32_t s = samples[i];
        sum_sq += s * s;
    }
    float rms = sqrtf((float)sum_sq / (float)count);

    /* ZCR — cruzamentos de zero normalizados */
    uint32_t zcr_count = 0;
    for (size_t i = 1; i < count; i++) {
        if ((samples[i - 1] >= 0) != (samples[i] >= 0)) zcr_count++;
    }
    float zcr = (float)zcr_count / (float)(count - 1u);

    /* energy_db */
    result.energy_db = (rms > 0.5f) ? (20.0f * log10f(rms / 32767.0f)) : -96.0f;

    /* Decisão: RMS + ZCR */
    result.is_speech = (rms   > (float)s_rms_threshold) &&
                       (zcr   > ZCR_MIN)                &&
                       (zcr   < ZCR_MAX);

    return result;
}

void vad_suppress_ms(uint32_t ms)
{
    s_suppress_until_ms = now_ms() + ms;
}
