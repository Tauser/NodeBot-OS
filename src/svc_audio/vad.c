#include "vad.h"
#include "event_bus.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>
#include <string.h>

static const char *TAG = "VAD";

/* ── Configuração ──────────────────────────────────────────────────── */
#define VAD_NVS_NS        "nodebot_vad"
#define VAD_NVS_KEY_THR   "rms_thr"
#define VAD_RMS_DEFAULT   200          /* substituído pelo adaptativo      */

#define VAD_ZCR_MIN       0.02f
#define VAD_ZCR_MAX       0.60f

/* Calibração automática de ruído de fundo */
#define VAD_WARMUP_BLOCKS 100u         /* ~3 s (100 × 32 ms)               */
#define VAD_NOISE_FACTOR  2.0f         /* threshold = ruído × fator        */
#define VAD_THR_MIN       300u         /* nunca abaixo deste valor         */

/* Confirmação de silêncio */
#define VAD_SILENCE_CONFIRM  5u        /* blocos para confirmar fim de fala (~160ms) */

/* ── Estado ────────────────────────────────────────────────────────── */
static uint16_t         s_rms_nvs        = VAD_RMS_DEFAULT;
static uint16_t         s_rms_threshold  = VAD_RMS_DEFAULT;
static bool             s_last_is_speech = false;
static volatile int64_t s_suppress_until = 0;
/* Inicia confirmado como silêncio para não disparar "FALA" falso no boot */
static uint8_t          s_silence_ticks  = 5u;  /* deve coincidir com VAD_SILENCE_CONFIRM */

/* Warmup / calibração */
static uint32_t s_warmup_count = 0;
static uint32_t s_warmup_min   = UINT32_MAX;  /* RMS mínimo observado     */
static bool     s_calibrated   = false;

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

    /* Remove DC bias antes de calcular ZCR */
    int32_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += buf[i];
    int16_t mean = (int16_t)(sum / (int32_t)n);

    uint32_t crossings = 0;
    int16_t prev = buf[0] - mean;
    for (size_t i = 1; i < n; i++) {
        int16_t cur = buf[i] - mean;
        if ((cur >= 0) != (prev >= 0)) crossings++;
        prev = cur;
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
            s_rms_nvs = thr;
        }
        nvs_close(h);
    }
    /* threshold inicial = NVS; será sobrescrito após warmup */
    s_rms_threshold = s_rms_nvs;

    ESP_LOGI(TAG, "VAD init — calibrando ruído de fundo por ~%u s...",
             (unsigned)(VAD_WARMUP_BLOCKS * 32 / 1000));
}

/* ── Process block ─────────────────────────────────────────────────── */

void vad_process_block(const int16_t *samples, size_t n)
{
    if (!samples || n == 0) return;

    const uint32_t rms = calc_rms(samples, n);
    const float    zcr = calc_zcr(samples, n);

    /* ── Fase de warmup: estima o ruído de fundo ──────────────────── */
    if (!s_calibrated) {
        if (rms < s_warmup_min) s_warmup_min = rms;
        s_warmup_count++;

        if (s_warmup_count >= VAD_WARMUP_BLOCKS) {
            /* Usa o mínimo observado — mais robusto que média em ambientes ruidosos */
            uint32_t noise_floor = s_warmup_min;
            uint16_t adaptive    = (uint16_t)(noise_floor * VAD_NOISE_FACTOR);
            /* usa o maior entre o adaptativo e o valor do NVS */
            s_rms_threshold = (adaptive > s_rms_nvs) ? adaptive : s_rms_nvs;
            if (s_rms_threshold < VAD_THR_MIN) s_rms_threshold = VAD_THR_MIN;
            s_calibrated = true;
            ESP_LOGI(TAG,
                     "VAD calibrado — noise_floor=%u  threshold=%u  (nvs=%u)",
                     (unsigned)noise_floor,
                     (unsigned)s_rms_threshold,
                     (unsigned)s_rms_nvs);
        }
        return;   /* não publica eventos durante warmup */
    }

    /* ── Log periódico (debug) — remover em produção ──────────────── */
    static uint8_t s_log_div = 0;
    if (++s_log_div >= 20) {
        s_log_div = 0;
        ESP_LOGI(TAG, "rms=%u thr=%u  zcr=%.3f",
                 (unsigned)rms, (unsigned)s_rms_threshold, (double)zcr);
    }

    /* ── Supressão durante playback ───────────────────────────────── */
    bool suppressed = (esp_timer_get_time() < s_suppress_until);

    bool is_speech = false;
    if (!suppressed) {
        bool voice = (rms > s_rms_threshold)
                  && (zcr > VAD_ZCR_MIN)
                  && (zcr < VAD_ZCR_MAX);

        if (voice) {
            s_silence_ticks = 0;
            is_speech = true;
        } else {
            if (s_silence_ticks < VAD_SILENCE_CONFIRM) s_silence_ticks++;
            is_speech = (s_silence_ticks < VAD_SILENCE_CONFIRM);
        }
    }

    /* ── Publica apenas quando o estado muda ──────────────────────── */
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

    ESP_LOGI(TAG, "VAD %s  rms=%u  zcr=%.3f  db=%.1f",
             is_speech ? ">>> FALA <<<" : "    silêncio",
             (unsigned)rms, (double)zcr, (double)energy_db);
}

/* ── Suppression gate ──────────────────────────────────────────────── */

void vad_suppress_ms(uint32_t ms)
{
    s_suppress_until = esp_timer_get_time() + (int64_t)ms * 1000LL;
}
