#include "intent_mapper.h"
#include "audio_capture.h"
#include "cloud_bridge.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

static const char *TAG = "intent";

/* ── Captura ────────────────────────────────────────────────────────────── */
#define CAPTURE_MAX_SAMPLES    (16000 * 3)   /* 3s @ 16kHz               */
#define SILENCE_THRESHOLD_SQ   (200 * 200)   /* RMS² mín (~-43 dBFS)     */
#define SILENCE_BLOCKS_MIN      6            /* ~192ms de silêncio        */
#define SPEECH_BLOCKS_MIN       3            /* mín fala antes de parar   */
#define SPEECH_MAX_BLOCKS      47            /* máx 1.5s de fala          */
#define CAPTURE_SUPPRESS_SAMPLES (16000u * 200u / 1000u)  /* 200ms       */
#define TASK_STACK             4096u
#define TASK_PRIO               13u

static int16_t          *s_capture_buf     = NULL;
static volatile size_t   s_capture_idx     = 0;
static volatile bool     s_capturing       = false;
static volatile size_t   s_speech_end_idx  = 0;
static volatile bool     s_speech_started  = false;
static int               s_speech_count    = 0;
static int               s_silence_count   = 0;
static SemaphoreHandle_t s_done_sem        = NULL;

/* ── Text-to-intent (substring case-insensitive) ────────────────────────── */
typedef struct { const char *kw; intent_t intent; } kw_map_t;

static const kw_map_t s_map[] = {
    { "dorme",      INTENT_SLEEP        },
    { "dormir",     INTENT_SLEEP        },
    { "acorda",     INTENT_WAKE         },
    { "acordar",    INTENT_WAKE         },
    { "sil",        INTENT_SILENCE      }, /* silêncio, silencio */
    { "privad",     INTENT_PRIVACY_MODE }, /* privado, privacidade */
    { "que hora",   INTENT_WHAT_TIME    },
    { "horas",      INTENT_WHAT_TIME    },
    { "como voc",   INTENT_HOW_ARE_YOU  },
    { "como vai",   INTENT_HOW_ARE_YOU  },
    { "olhe",       INTENT_LOOK_AT_ME   },
    { "olha",       INTENT_LOOK_AT_ME   },
    { "me olh",     INTENT_LOOK_AT_ME   },
    { "vol",        INTENT_VOLUME_UP    }, /* resolvido abaixo por "baixo" */
    { "aumenta",    INTENT_VOLUME_UP    },
    { "mais alto",  INTENT_VOLUME_UP    },
    { "diminui",    INTENT_VOLUME_DOWN  },
    { "mais baixo", INTENT_VOLUME_DOWN  },
    { "volume bai", INTENT_VOLUME_DOWN  },
    { "sim",        INTENT_YES          },
    { "n\xc3\xa3o", INTENT_NO          }, /* não */
    { "nao",        INTENT_NO           },
    { "cancela",    INTENT_CANCEL       },
    { "cancelar",   INTENT_CANCEL       },
};

static intent_t text_to_intent(const char *text)
{
    if (!text || text[0] == '\0') return INTENT_UNKNOWN;

    /* Converte para minúsculas numa cópia local */
    char low[128];
    size_t len = strlen(text);
    if (len >= sizeof(low)) len = sizeof(low) - 1;
    for (size_t i = 0; i < len; i++) low[i] = (char)tolower((unsigned char)text[i]);
    low[len] = '\0';

    /* "volume baixo" tem prioridade sobre "vol" genérico */
    if (strstr(low, "volume bai") || strstr(low, "mais baixo") || strstr(low, "diminui"))
        return INTENT_VOLUME_DOWN;

    for (size_t i = 0; i < sizeof(s_map) / sizeof(s_map[0]); i++) {
        if (strstr(low, s_map[i].kw)) return s_map[i].intent;
    }
    return INTENT_UNKNOWN;
}

/* ── STT callback (chamado de CloudTask, Core 0) ─────────────────────────── */
static void on_stt_result(const char *transcript)
{
    intent_t intent = text_to_intent(transcript);
    intent_event_t evt = {
        .intent     = (uint8_t)intent,
        .confidence = (transcript && transcript[0]) ? 90u : 0u,
    };
    ESP_LOGI(TAG, "Whisper: \"%s\" → intent=%d",
             transcript ? transcript : "(null)", intent);
    event_bus_publish(EVT_INTENT_DETECTED, &evt, sizeof(evt), EVENT_PRIO_BEHAVIOR);
}

/* ── PCM listener ───────────────────────────────────────────────────────── */
static void on_pcm_block(const int16_t *pcm, size_t len)
{
    if (!s_capturing) return;

    int64_t sq = 0;
    for (size_t i = 0; i < len; i++) { int32_t v = pcm[i]; sq += v * v; }
    uint32_t rms_sq = (uint32_t)(sq / (int64_t)len);

    bool in_suppress = (s_capture_idx < CAPTURE_SUPPRESS_SAMPLES);

    if (!in_suppress && rms_sq >= (uint32_t)SILENCE_THRESHOLD_SQ) {
        if (!s_speech_started) {
            s_speech_started = true;
            ESP_LOGD(TAG, "fala detectada @ %u samples", (unsigned)s_capture_idx);
        }
        size_t end_candidate = s_capture_idx + len + 1600u; /* +100ms */
        if (end_candidate > (size_t)CAPTURE_MAX_SAMPLES)
            end_candidate = (size_t)CAPTURE_MAX_SAMPLES;
        if (end_candidate > s_speech_end_idx)
            s_speech_end_idx = end_candidate;
        s_speech_count++;
        s_silence_count = 0;
    } else if (!in_suppress) {
        s_silence_count++;
    }

    size_t space = (size_t)CAPTURE_MAX_SAMPLES - s_capture_idx;
    size_t copy  = len < space ? len : space;
    if (copy > 0) {
        memcpy(s_capture_buf + s_capture_idx, pcm, copy * sizeof(int16_t));
        s_capture_idx += copy;
    }

    bool end_speech      = s_speech_started &&
                           (s_speech_count  >= SPEECH_BLOCKS_MIN) &&
                           (s_silence_count >= SILENCE_BLOCKS_MIN);
    bool speech_too_long = (s_speech_count  >= SPEECH_MAX_BLOCKS);
    bool buf_full        = (s_capture_idx   >= (size_t)CAPTURE_MAX_SAMPLES);

    if (end_speech || speech_too_long || buf_full) {
        s_capturing = false;
        audio_capture_set_pcm_listener(NULL);
        xSemaphoreGive(s_done_sem);
    }
}

/* ── Inicia captura ─────────────────────────────────────────────────────── */
static void start_capture(void)
{
    s_capture_idx    = 0;
    s_speech_count   = 0;
    s_silence_count  = 0;
    s_speech_end_idx = 0;
    s_speech_started = false;
    s_capturing      = true;
    audio_capture_set_pcm_listener(on_pcm_block);
    ESP_LOGI(TAG, "captura iniciada (supressão %ums)",
             (unsigned)(CAPTURE_SUPPRESS_SAMPLES * 1000u / 16000u));
}

/* ── Callback EVT_WAKE_WORD ─────────────────────────────────────────────── */
static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    if (s_capturing) return;
    start_capture();
}

/* ── Teste direto (sem wake word) ───────────────────────────────────────── */
void intent_mapper_test_capture(void)
{
    if (s_capturing) { ESP_LOGW(TAG, "captura já em andamento"); return; }
    ESP_LOGI(TAG, "=== TESTE — fale uma keyword ===");
    start_capture();
}

/* ── Task de processamento ──────────────────────────────────────────────── */
static void intent_mapper_task(void *arg)
{
    (void)arg;
    while (1) {
        xSemaphoreTake(s_done_sem, portMAX_DELAY);

        size_t speech_start = 0;
        size_t speech_end   = s_speech_end_idx > 0 ? s_speech_end_idx : s_capture_idx;
        if (speech_end > s_capture_idx) speech_end = s_capture_idx;
        size_t speech_len   = speech_end - speech_start;

        ESP_LOGI(TAG, "captura: total=%u  fala=[%u..%u] (%ums)",
                 (unsigned)s_capture_idx,
                 (unsigned)speech_start, (unsigned)speech_end,
                 (unsigned)(speech_len * 1000u / 16000u));

        if (cloud_bridge_is_available() && speech_len > 0) {
            /* Copia para o buffer do cloud_bridge (ele tem o próprio buffer interno) */
            cloud_bridge_request_stt(s_capture_buf + speech_start, speech_len,
                                     on_stt_result);
        } else {
            ESP_LOGW(TAG, "cloud indisponível — INTENT_UNKNOWN");
            intent_event_t evt = { .intent = INTENT_UNKNOWN, .confidence = 0 };
            event_bus_publish(EVT_INTENT_DETECTED, &evt, sizeof(evt),
                              EVENT_PRIO_BEHAVIOR);
        }
    }
}

/* ── API pública ─────────────────────────────────────────────────────────── */

intent_t intent_mapper_resolve(int keyword_id)
{
    (void)keyword_id;
    return INTENT_UNKNOWN;   /* sem KWS local — usar Whisper */
}

esp_err_t intent_mapper_init(void)
{
    s_capture_buf = heap_caps_malloc(
        (size_t)CAPTURE_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!s_capture_buf) {
        ESP_LOGE(TAG, "sem PSRAM para capture_buf");
        return ESP_ERR_NO_MEM;
    }

    s_done_sem = xSemaphoreCreateBinary();
    if (!s_done_sem) {
        heap_caps_free(s_capture_buf);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = event_bus_subscribe(EVT_WAKE_WORD, on_wake_word);
    if (err != ESP_OK) return err;

    BaseType_t rc = xTaskCreatePinnedToCore(
        intent_mapper_task, "intent_mapper",
        TASK_STACK, NULL, TASK_PRIO, NULL, 1 /* Core 1 */);
    if (rc != pdPASS) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "ok — Whisper STT, Core1 P%u, capture=%uKB",
             TASK_PRIO,
             (unsigned)(CAPTURE_MAX_SAMPLES * sizeof(int16_t) / 1024u));
    return ESP_OK;
}
