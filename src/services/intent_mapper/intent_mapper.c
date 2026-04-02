#include "intent_mapper.h"
#include "keyword_spotter.h"
#include "audio_capture.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "intent";

/* ── Constantes de captura ─────────────────────────────────────────────── */
#define CAPTURE_MAX_SAMPLES    (16000 * 3)   /* 3s @ 16kHz = 48 000 amostras */
#define SILENCE_THRESHOLD_SQ   (500 * 500)   /* RMS² mínimo para fala (~-36 dBFS) */
#define SILENCE_BLOCKS_MIN      16           /* ~512 ms de silêncio (16×32ms) */
#define SPEECH_BLOCKS_MIN        5           /* mín fala antes de parar       */
#define TASK_STACK              8192u
#define TASK_PRIO                 13u   /* acima de Behavior P12, abaixo de EventDispatch P15 */
#define KWS_TEMPLATES_DIR       "/sdcard/kws"

/* ── Estado de captura (escrito em Core 0, lido em Core 1) ─────────────── */
static int16_t          *s_capture_buf        = NULL;
static volatile size_t   s_capture_idx        = 0;
static volatile bool     s_capturing          = false;
static volatile size_t   s_speech_start_idx   = 0;   /* início da fala no buffer */
static volatile size_t   s_speech_end_idx     = 0;   /* fim da fala no buffer    */
static volatile bool     s_speech_started     = false;
static int               s_speech_count       = 0;
static int               s_silence_count      = 0;
static SemaphoreHandle_t s_done_sem           = NULL;

/* Timer para atrasar captura após wake word — aguarda WHOOSH terminar */
#define CAPTURE_DELAY_MS   250u   /* duração do WHOOSH (~200ms) + margem */
static esp_timer_handle_t s_capture_timer     = NULL;

/* ── Mapeamento keyword_id → intent_t ──────────────────────────────────── */
static const intent_t s_kw_to_intent[KWS_KEYWORDS] = {
    [0]  = INTENT_SLEEP,
    [1]  = INTENT_WAKE,
    [2]  = INTENT_SILENCE,
    [3]  = INTENT_PRIVACY_MODE,
    [4]  = INTENT_WHAT_TIME,
    [5]  = INTENT_HOW_ARE_YOU,
    [6]  = INTENT_LOOK_AT_ME,
    [7]  = INTENT_VOLUME_UP,
    [8]  = INTENT_VOLUME_DOWN,
    [9]  = INTENT_YES,
    [10] = INTENT_NO,
    [11] = INTENT_CANCEL,
};

/* ── PCM listener — chamado de audio_capture_task (Core 0) ─────────────── */
static void on_pcm_block(const int16_t *pcm, size_t len)
{
    if (!s_capturing) return;

    /* Energia do bloco sem sqrt (comparação quadrática) */
    int64_t sq = 0;
    for (size_t i = 0; i < len; i++) {
        int32_t v = pcm[i];
        sq += v * v;
    }
    uint32_t rms_sq = (uint32_t)(sq / (int64_t)len);

    if (rms_sq >= (uint32_t)SILENCE_THRESHOLD_SQ) {
        if (!s_speech_started) {
            /* Recua 50 ms para não cortar o onset da keyword */
            size_t onset_pad = 800; /* 800 samples = 50 ms @ 16kHz */
            s_speech_start_idx = (s_capture_idx > onset_pad)
                                 ? s_capture_idx - onset_pad : 0;
            s_speech_started = true;
        }
        /* Atualiza fim de fala com 100 ms de margem após o bloco atual */
        size_t tail_pad = 1600; /* 100 ms @ 16kHz */
        size_t end_candidate = s_capture_idx + len + tail_pad;
        if (end_candidate > (size_t)CAPTURE_MAX_SAMPLES)
            end_candidate = (size_t)CAPTURE_MAX_SAMPLES;
        if (end_candidate > s_speech_end_idx)
            s_speech_end_idx = end_candidate;
        s_speech_count++;
        s_silence_count = 0;
    } else {
        s_silence_count++;
    }

    /* Copia amostras para o buffer de captura */
    size_t space = (size_t)CAPTURE_MAX_SAMPLES - s_capture_idx;
    size_t copy  = len < space ? len : space;
    if (copy > 0) {
        memcpy(s_capture_buf + s_capture_idx, pcm, copy * sizeof(int16_t));
        s_capture_idx += copy;
    }

    /* Verifica condição de parada */
    bool end_speech  = (s_speech_count  >= SPEECH_BLOCKS_MIN) &&
                       (s_silence_count >= SILENCE_BLOCKS_MIN);
    bool buf_full    = (s_capture_idx   >= (size_t)CAPTURE_MAX_SAMPLES);

    if (end_speech || buf_full) {
        s_capturing = false;
        audio_capture_set_pcm_listener(NULL);
        xSemaphoreGive(s_done_sem);
    }
}

/* ── Timer callback — inicia captura após WHOOSH terminar ───────────────── */
static void capture_start_cb(void *arg)
{
    (void)arg;
    s_capture_idx      = 0;
    s_speech_count     = 0;
    s_silence_count    = 0;
    s_speech_start_idx = 0;
    s_speech_end_idx   = 0;
    s_speech_started   = false;
    s_capturing        = true;
    audio_capture_set_pcm_listener(on_pcm_block);
    ESP_LOGD(TAG, "captura iniciada (pós-WHOOSH)");
}

/* ── Callback EVT_WAKE_WORD (Core 1 dispatch task) ─────────────────────── */
static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    if (s_capturing) return;   /* já em captura */

    /* Aguarda WHOOSH terminar antes de abrir o listener de PCM.
     * Sem delay, o som do speaker contamina o buffer de captura e o DTW
     * reconhece palavras erradas. */
    esp_timer_start_once(s_capture_timer,
                         (uint64_t)CAPTURE_DELAY_MS * 1000u);
    ESP_LOGD(TAG, "wake word — captura em %ums", CAPTURE_DELAY_MS);
}

/* ── Task de processamento (Core 1, P10) ────────────────────────────────── */
static void intent_mapper_task(void *arg)
{
    (void)arg;
    while (1) {
        xSemaphoreTake(s_done_sem, portMAX_DELAY);

        size_t captured   = s_capture_idx;
        size_t speech_off = s_speech_start_idx;
        size_t speech_end = s_speech_end_idx;
        /* Se não detectou fala: passa o buffer inteiro como fallback */
        if (!s_speech_started || speech_end <= speech_off) {
            speech_off = 0;
            speech_end = captured;
        }
        size_t speech_len = speech_end - speech_off;
        ESP_LOGI(TAG, "captura: total=%u  fala=[%u..%u] (%u amostras)",
                 (unsigned)captured, (unsigned)speech_off,
                 (unsigned)speech_end, (unsigned)speech_len);

        kws_result_t r = keyword_spotter_match(s_capture_buf + speech_off, speech_len);

        intent_t intent = intent_mapper_resolve(r.keyword_id);
        intent_event_t evt = {
            .intent     = (uint8_t)intent,
            .confidence = (uint8_t)(r.confidence * 100.0f),
        };

        ESP_LOGI(TAG, "intent=%d conf=%u%% kw=%d(%s)",
                 evt.intent, evt.confidence,
                 r.keyword_id, keyword_spotter_name(r.keyword_id));

        event_bus_publish(EVT_INTENT_DETECTED, &evt, sizeof(evt),
                          EVENT_PRIO_BEHAVIOR);
    }
}

/* ── API pública ─────────────────────────────────────────────────────────── */

intent_t intent_mapper_resolve(int keyword_id)
{
    if (keyword_id < 0 || keyword_id >= KWS_KEYWORDS) return INTENT_UNKNOWN;
    return s_kw_to_intent[keyword_id];
}

esp_err_t intent_mapper_init(void)
{
    /* Buffer de captura em PSRAM */
    s_capture_buf = heap_caps_malloc(
        (size_t)CAPTURE_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!s_capture_buf) {
        ESP_LOGE(TAG, "sem PSRAM para capture_buf (%u bytes)",
                 (unsigned)(CAPTURE_MAX_SAMPLES * sizeof(int16_t)));
        return ESP_ERR_NO_MEM;
    }

    s_done_sem = xSemaphoreCreateBinary();
    if (!s_done_sem) {
        heap_caps_free(s_capture_buf);
        s_capture_buf = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* keyword_spotter: falha tolerada (sem templates = INTENT_UNKNOWN) */
    esp_err_t kws_err = keyword_spotter_init(KWS_TEMPLATES_DIR);
    if (kws_err != ESP_OK) {
        ESP_LOGW(TAG, "keyword_spotter sem templates — INTENT_UNKNOWN sempre");
    }

    /* Timer one-shot para atrasar início de captura pós-wake-word */
    esp_timer_create_args_t timer_args = {
        .callback = capture_start_cb,
        .arg      = NULL,
        .name     = "intent_cap",
    };
    esp_err_t terr = esp_timer_create(&timer_args, &s_capture_timer);
    if (terr != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create falhou: %d", terr);
        return terr;
    }

    /* Subscreve EVT_WAKE_WORD */
    esp_err_t err = event_bus_subscribe(EVT_WAKE_WORD, on_wake_word);
    if (err != ESP_OK) return err;

    /* Task de processamento */
    BaseType_t rc = xTaskCreatePinnedToCore(
        intent_mapper_task, "intent_mapper",
        TASK_STACK, NULL, TASK_PRIO, NULL, 1 /* Core 1 */);
    if (rc != pdPASS) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "ok — Core1 P%u capture=%uKB",
             TASK_PRIO,
             (unsigned)(CAPTURE_MAX_SAMPLES * sizeof(int16_t) / 1024u));
    return ESP_OK;
}
