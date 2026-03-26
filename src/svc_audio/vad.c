#include "vad.h"
#include "event_bus.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "VAD";

/* ── Configuração ──────────────────────────────────────────────────── */
#define VAD_NVS_NS        "nodebot_vad"
#define VAD_NVS_KEY_THR   "rms_thr"
#define VAD_RMS_DEFAULT   200          /* [0, 32767] */

#define VAD_ZCR_MIN       0.05f
#define VAD_ZCR_MAX       0.45f

/* ── Estado ────────────────────────────────────────────────────────── */
static uint16_t           s_rms_threshold  = VAD_RMS_DEFAULT;
static bool               s_last_is_speech = false;
static volatile int64_t   s_suppress_until = 0;   /* µs, via esp_timer_get_time() */

/* ── Helpers ───────────────────────────────────────────────────────── */

static uint32_t calc_rms(const int16_t *buf, size_t n)
{
    int64_t sum_sq = 0;
    for (size_t i = 0; i < n; i++) {
        int64_t v = buf[i];
        sum_sq += v * v;
    }
    return (uint32_t)sqrtf((float)(sum_sq / (int64_t)n));
}

static float calc_zcr(const int16_t *buf, size_t n)
{
    if (n < 2u) return 0.0f;
    uint32_t crossings = 0;
    for (size_t i = 1; i < n; i++) {
        if ((buf[i] >= 0) != (buf[i - 1] >= 0)) crossings++;
    }
    return (float)crossings / (float)(n - 1u);
}

/* ── Init ──────────────────────────────────────────────────────────── */

void vad_init(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(VAD_NVS_NS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        uint16_t thr = 0;
        if (nvs_get_u16(h, VAD_NVS_KEY_THR, &thr) == ESP_OK && thr > 0) {
            s_rms_threshold = thr;
        }
        nvs_close(h);
    }

    ESP_LOGI(TAG, "VAD init  rms_thr=%u  zcr=[%.2f, %.2f]",
             (unsigned)s_rms_threshold, (double)VAD_ZCR_MIN, (double)VAD_ZCR_MAX);
}

/* ── Process block ─────────────────────────────────────────────────── */

void vad_process_block(const int16_t *samples, size_t n)
{
    if (!samples || n == 0) return;

    const uint32_t rms = calc_rms(samples, n);
    const float    zcr = calc_zcr(samples, n);

    /* Supressão durante playback */
    bool suppressed = (esp_timer_get_time() < s_suppress_until);

    bool is_speech = false;
    if (!suppressed) {
        is_speech = (rms > s_rms_threshold)
                 && (zcr > VAD_ZCR_MIN)
                 && (zcr < VAD_ZCR_MAX);
    }

    /* Publica apenas quando o estado muda */
    if (is_speech == s_last_is_speech) return;
    s_last_is_speech = is_speech;

    const float energy_db = (rms > 0)
        ? 20.0f * log10f((float)rms / 32767.0f)
        : -90.0f;

    voice_activity_event_t evt = {
        .energy_db = energy_db,
        .is_speech  = is_speech,
    };

    event_bus_publish(EVT_VOICE_ACTIVITY, &evt, sizeof(evt), EVENT_PRIO_SYSTEM);

    ESP_LOGD(TAG, "VAD %s  rms=%u  zcr=%.3f  db=%.1f",
             is_speech ? "SPEECH" : "SILENCE",
             (unsigned)rms, (double)zcr, (double)energy_db);
}

/* ── Suppression gate ──────────────────────────────────────────────── */

void vad_suppress_ms(uint32_t ms)
{
    s_suppress_until = esp_timer_get_time() + (int64_t)ms * 1000LL;
}
