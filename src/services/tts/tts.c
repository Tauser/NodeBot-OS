#include "tts.h"
#include "audio_driver.h"
#include "audio_feedback.h"
#include "wake_word.h"
#include "vad.h"
#include "sd_driver.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "tts";

/* ── Configuração ──────────────────────────────────────────────────────── */
#define TASK_STACK          3072u
#define TASK_PRIO             16u
#define CMD_QUEUE_DEPTH        8u
#define MAX_PHRASE_SAMPLES  (16000u * 6u)   /* máx 6 s @ 16 kHz por frase   */
#define WAKE_SUPPRESS_MS      800u
#define VAD_SUPPRESS_MS       800u

#define N_HOURS 24u

/* ── Tipos internos ────────────────────────────────────────────────────── */
#define CMD_TYPE_PHRASE   0u
#define CMD_TYPE_DYNAMIC  1u

typedef struct {
    uint8_t type;    /* CMD_TYPE_PHRASE ou CMD_TYPE_DYNAMIC */
    uint8_t id;      /* phrase_id_t ou tts_template_t       */
    int16_t val;     /* valor para templates dinâmicos      */
} tts_cmd_t;

/* ── Mapeamento de arquivos no SD ──────────────────────────────────────── */
static const char *k_phrase_paths[PHRASE_COUNT] = {
    [PHRASE_NOT_UNDERSTOOD]    = "/sdcard/tts/nao_entendi.pcm",
    [PHRASE_TIMEOUT_LISTENING] = "/sdcard/tts/pode_repetir.pcm",
    [PHRASE_OK]                = "/sdcard/tts/ok.pcm",
    [PHRASE_SLEEP]             = "/sdcard/tts/vou_dormir.pcm",
    [PHRASE_WAKE]              = "/sdcard/tts/estou_acordado.pcm",
    [PHRASE_SILENCE]           = "/sdcard/tts/silencio.pcm",
    [PHRASE_HOW_ARE_YOU]       = "/sdcard/tts/estou_bem.pcm",
    [PHRASE_LOOK_AT_ME]        = "/sdcard/tts/te_vejo.pcm",
    [PHRASE_VOLUME_UP]         = "/sdcard/tts/volume_alto.pcm",
    [PHRASE_VOLUME_DOWN]       = "/sdcard/tts/volume_baixo.pcm",
    [PHRASE_YES]               = "/sdcard/tts/sim.pcm",
    [PHRASE_NO]                = "/sdcard/tts/nao.pcm",
    [PHRASE_CANCEL]            = "/sdcard/tts/cancelando.pcm",
};

/* ── Buffers em PSRAM ─────────────────────────────────────────────────── */
typedef struct {
    int16_t *buf;
    size_t   samples;
} phrase_buf_t;

static phrase_buf_t s_phrases[PHRASE_COUNT];
static phrase_buf_t s_hours[N_HOURS];
static QueueHandle_t s_queue = NULL;

/* ── Helpers de carregamento ────────────────────────────────────────────── */

/* Detecta offset de dados em arquivo WAV ou PCM cru. */
static size_t wav_data_offset(const uint8_t *raw, size_t len)
{
    if (len >= 44u && raw[0] == 'R' && raw[1] == 'I' &&
                      raw[2] == 'F' && raw[3] == 'F') {
        for (size_t i = 12u; i + 8u <= len; i += 4u) {
            if (raw[i]   == 'd' && raw[i+1] == 'a' &&
                raw[i+2] == 't' && raw[i+3] == 'a') {
                return i + 8u;
            }
        }
        return 44u;
    }
    return 0u;
}

/*
 * Tenta carregar `path` (.pcm) ou variante .wav para `out`.
 * Retorna true em caso de sucesso.
 */
static bool load_pcm(const char *path, phrase_buf_t *out)
{
    /* 1. Tenta .pcm; se não existir, troca sufixo para .wav */
    char alt[64];
    const char *p = path;

    if (!sd_file_exists(p)) {
        size_t n = strlen(path);
        if (n > 4u && n < sizeof(alt)) {
            memcpy(alt, path, n + 1u);
            memcpy(alt + n - 4u, ".wav", 5u);
            if (!sd_file_exists(alt)) return false;
            p = alt;
        } else {
            return false;
        }
    }

    size_t max_bytes = MAX_PHRASE_SAMPLES * sizeof(int16_t);
    uint8_t *raw = heap_caps_malloc(max_bytes, MALLOC_CAP_SPIRAM);
    if (!raw) return false;

    size_t bytes = sd_read_file(p, raw, max_bytes);
    if (bytes < 2u) { heap_caps_free(raw); return false; }

    size_t offset  = wav_data_offset(raw, bytes);
    size_t samples = (bytes - offset) / sizeof(int16_t);
    if (samples == 0u) { heap_caps_free(raw); return false; }

    if (offset > 0u) memmove(raw, raw + offset, samples * sizeof(int16_t));

    if (out->buf) heap_caps_free(out->buf);
    out->buf     = (int16_t *)raw;
    out->samples = samples;
    return true;
}

/* ── Reprodução ─────────────────────────────────────────────────────────── */

static void play_buf(const phrase_buf_t *b)
{
    if (!b || !b->buf || b->samples == 0u) {
        /* Sem áudio carregado: suprime wake word/VAD antes do beep para evitar
         * que o speaker re-acorde o sistema em loop */
        wake_word_suppress_ms(WAKE_SUPPRESS_MS);
        vad_suppress_ms(VAD_SUPPRESS_MS);
        audio_feedback_play(SOUND_BEEP_ACK);
        vTaskDelay(pdMS_TO_TICKS(250));
        return;
    }
    wake_word_suppress_ms(WAKE_SUPPRESS_MS);
    vad_suppress_ms(VAD_SUPPRESS_MS);
    audio_play_pcm(b->buf, b->samples);
}

/* ── Task TTS ───────────────────────────────────────────────────────────── */

static void tts_task(void *arg)
{
    (void)arg;
    tts_cmd_t cmd;

    while (1) {
        if (xQueueReceive(s_queue, &cmd, portMAX_DELAY) != pdTRUE) continue;

        if (cmd.type == CMD_TYPE_PHRASE) {
            if (cmd.id < PHRASE_COUNT) {
                play_buf(&s_phrases[cmd.id]);
            }
        } else if (cmd.type == CMD_TYPE_DYNAMIC) {
            if (cmd.id == (uint8_t)TPL_HOUR) {
                int h = cmd.val;
                if (h < 0 || h >= (int)N_HOURS) h = 0;
                play_buf(&s_hours[h]);
            }
        }

        event_bus_publish(EVT_TTS_DONE, NULL, 0, EVENT_PRIO_BEHAVIOR);
    }
}

/* ── API pública ────────────────────────────────────────────────────────── */

void tts_play_phrase(phrase_id_t id)
{
    if (!s_queue || id >= PHRASE_COUNT) return;
    const tts_cmd_t cmd = { .type = CMD_TYPE_PHRASE, .id = (uint8_t)id, .val = 0 };
    xQueueSend(s_queue, &cmd, 0);
}

void tts_play_dynamic(tts_template_t tpl, int val)
{
    if (!s_queue || tpl >= TPL_COUNT) return;
    const tts_cmd_t cmd = {
        .type = CMD_TYPE_DYNAMIC,
        .id   = (uint8_t)tpl,
        .val  = (int16_t)val,
    };
    xQueueSend(s_queue, &cmd, 0);
}

esp_err_t tts_init(void)
{
    memset(s_phrases, 0, sizeof(s_phrases));
    memset(s_hours,   0, sizeof(s_hours));

    /* Carrega frases estáticas do SD */
    int loaded = 0;
    for (int i = 0; i < PHRASE_COUNT; i++) {
        if (load_pcm(k_phrase_paths[i], &s_phrases[i])) {
            ESP_LOGI(TAG, "frase %d: %u samples", i, (unsigned)s_phrases[i].samples);
            loaded++;
        } else {
            ESP_LOGW(TAG, "frase %d: não encontrada — usará BEEP_ACK", i);
        }
    }

    /* Carrega arquivos de hora (TPL_HOUR): hora_0.pcm .. hora_23.pcm */
    char path[48];
    for (unsigned h = 0u; h < N_HOURS; h++) {
        snprintf(path, sizeof(path), "/sdcard/tts/hora_%u.pcm", h);
        if (load_pcm(path, &s_hours[h])) {
            ESP_LOGD(TAG, "hora_%u: %u samples", h, (unsigned)s_hours[h].samples);
        }
    }

    s_queue = xQueueCreate(CMD_QUEUE_DEPTH, sizeof(tts_cmd_t));
    if (!s_queue) {
        ESP_LOGE(TAG, "falha ao criar fila");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        tts_task, "tts",
        TASK_STACK, NULL, TASK_PRIO, NULL, 1 /* Core 1 */);

    if (rc != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        ESP_LOGE(TAG, "falha ao criar task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ok — %d/%d frases do SD, P%u Core1",
             loaded, PHRASE_COUNT, TASK_PRIO);
    return ESP_OK;
}
