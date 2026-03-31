#include "audio_feedback.h"
#include "audio_driver.h"
#include "sd_driver.h"
#include "vad.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include <string.h>

static const char *TAG = "audio_fb";

/* ── Configuração ──────────────────────────────────────────────────────── */
#define TASK_STACK        3072u
#define TASK_PRIO           18u
#define CMD_QUEUE_DEPTH      4u
#define MAX_SOUND_SAMPLES (16000u * 2u)   /* máx 2s @ 16 kHz por som        */
#define VAD_SUPPRESS_MS    300u           /* suprime VAD durante playback    */

/* ── Mapeamento de arquivos no SD ──────────────────────────────────────── */
static const char *k_paths[SOUND_COUNT] = {
    [SOUND_BEEP_ACK]        = "/sdcard/sounds/beep_ack.pcm",
    [SOUND_DING_NOTIF]      = "/sdcard/sounds/ding_notif.pcm",
    [SOUND_WHOOSH_ACTIVATE] = "/sdcard/sounds/whoosh_ativate.pcm",
    [SOUND_CLICK_TOUCH]     = "/sdcard/sounds/click_touch.pcm",
    [SOUND_ERROR_TONE]      = "/sdcard/sounds/error_tone.pcm",
};

/* Fallback: frequência e duração para cada som */
static const struct { uint16_t freq_hz; uint32_t dur_ms; } k_fallback[SOUND_COUNT] = {
    [SOUND_BEEP_ACK]        = { 440,  100 },
    [SOUND_DING_NOTIF]      = { 880,  150 },
    [SOUND_WHOOSH_ACTIVATE] = { 660,  200 },
    [SOUND_CLICK_TOUCH]     = {1000,   50 },
    [SOUND_ERROR_TONE]      = { 220,  300 },
};

/* ── Buffers de som ────────────────────────────────────────────────────── */
typedef struct {
    int16_t *buf;
    size_t   samples;
} sound_buf_t;

static sound_buf_t   s_sounds[SOUND_COUNT];
static QueueHandle_t s_queue = NULL;

/* ── Carregamento ──────────────────────────────────────────────────────── */

static void load_fallback(sound_id_t id)
{
    uint32_t dur_ms  = k_fallback[id].dur_ms;
    uint16_t freq_hz = k_fallback[id].freq_hz;
    size_t   n       = (size_t)((uint64_t)16000u * dur_ms / 1000u);

    int16_t *buf = heap_caps_malloc(n * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!buf) {
        /* PSRAM indisponível — tenta DRAM */
        buf = heap_caps_malloc(n * sizeof(int16_t), MALLOC_CAP_DEFAULT);
    }
    if (!buf) {
        ESP_LOGW(TAG, "som %d: sem memória para fallback", (int)id);
        return;
    }

    audio_generate_beep(freq_hz, dur_ms, buf);
    s_sounds[id].buf     = buf;
    s_sounds[id].samples = n;
    ESP_LOGI(TAG, "som %d: fallback %uHz %ums", (int)id, freq_hz, (unsigned)dur_ms);
}

/* Tenta também com extensão .wav além de .pcm */
static const char *k_wav_paths[SOUND_COUNT] = {
    [SOUND_BEEP_ACK]        = "/sdcard/sounds/beep_ack.wav",
    [SOUND_DING_NOTIF]      = "/sdcard/sounds/ding_notif.wav",
    [SOUND_WHOOSH_ACTIVATE] = "/sdcard/sounds/whoosh_ativate.wav",
    [SOUND_CLICK_TOUCH]     = "/sdcard/sounds/click_touch.wav",
    [SOUND_ERROR_TONE]      = "/sdcard/sounds/error_tone.wav",
};

/* Retorna o offset de dados de um buffer lido do disco.
 * Se começar com "RIFF", pula o header WAV padrão (44 bytes).
 * Caso contrário assume PCM cru (offset 0). */
static size_t wav_data_offset(const uint8_t *raw, size_t len)
{
    if (len >= 44 && raw[0] == 'R' && raw[1] == 'I' &&
                     raw[2] == 'F' && raw[3] == 'F') {
        /* Localiza o chunk "data" — pode não estar exatamente em byte 36 */
        for (size_t i = 12; i + 8 <= len; i += 4) {
            if (raw[i]   == 'd' && raw[i+1] == 'a' &&
                raw[i+2] == 't' && raw[i+3] == 'a') {
                return i + 8;   /* pula id(4) + size(4) */
            }
        }
        return 44;  /* fallback: header WAV padrão */
    }
    return 0;   /* PCM cru */
}

static void try_load_from_sd(sound_id_t id)
{
    /* Aceita .pcm e .wav */
    const char *path = NULL;
    if      (sd_file_exists(k_paths[id]))     path = k_paths[id];
    else if (sd_file_exists(k_wav_paths[id])) path = k_wav_paths[id];
    else return;

    size_t max_bytes = MAX_SOUND_SAMPLES * sizeof(int16_t);
    uint8_t *raw = heap_caps_malloc(max_bytes, MALLOC_CAP_SPIRAM);
    if (!raw) return;

    size_t bytes = sd_read_file(path, raw, max_bytes);
    if (bytes < 2u) { heap_caps_free(raw); return; }

    size_t offset  = wav_data_offset(raw, bytes);
    size_t samples = (bytes - offset) / sizeof(int16_t);

    if (samples == 0) { heap_caps_free(raw); return; }

    /* Move dados para o início do buffer (in-place, offset pequeno) */
    if (offset > 0) memmove(raw, raw + offset, samples * sizeof(int16_t));

    if (s_sounds[id].buf) heap_caps_free(s_sounds[id].buf);
    s_sounds[id].buf     = (int16_t *)raw;
    s_sounds[id].samples = samples;
    ESP_LOGI(TAG, "som %d: SD '%s' %u samples (offset=%u)",
             (int)id, path, (unsigned)samples, (unsigned)offset);
}

/* ── Task de playback ──────────────────────────────────────────────────── */
static void audio_playback_task(void *arg)
{
    (void)arg;
    uint8_t cmd;

    while (1) {
        if (xQueueReceive(s_queue, &cmd, portMAX_DELAY) != pdTRUE) continue;

        if (cmd >= SOUND_COUNT) continue;
        sound_buf_t *s = &s_sounds[cmd];
        if (!s->buf || s->samples == 0) continue;

        vad_suppress_ms(VAD_SUPPRESS_MS);
        audio_play_pcm(s->buf, s->samples);
    }
}

/* ── API pública ───────────────────────────────────────────────────────── */

static void on_touch_press(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    audio_feedback_play(SOUND_CLICK_TOUCH);
}

static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    audio_feedback_play(SOUND_WHOOSH_ACTIVATE);
}

void audio_feedback_play(sound_id_t id)
{
    if (!s_queue || id >= SOUND_COUNT) return;
    uint8_t cmd = (uint8_t)id;
    /* xQueueSend com timeout 0 — nunca bloqueia */
    xQueueSend(s_queue, &cmd, 0);
}

esp_err_t audio_feedback_init(void)
{
    memset(s_sounds, 0, sizeof(s_sounds));

    /* 1. Gera fallbacks em PSRAM */
    for (int i = 0; i < SOUND_COUNT; i++) {
        load_fallback((sound_id_t)i);
    }

    /* 2. Sobrescreve com sons do SD se disponíveis */
    for (int i = 0; i < SOUND_COUNT; i++) {
        try_load_from_sd((sound_id_t)i);
    }

    /* 3. Fila de comandos */
    s_queue = xQueueCreate(CMD_QUEUE_DEPTH, sizeof(uint8_t));
    if (!s_queue) {
        ESP_LOGE(TAG, "falha ao criar fila");
        return ESP_ERR_NO_MEM;
    }

    /* 4. Task de playback */
    BaseType_t rc = xTaskCreatePinnedToCore(
        audio_playback_task, "audio_play",
        TASK_STACK, NULL, TASK_PRIO, NULL, 1 /* Core 1 */);

    if (rc != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        ESP_LOGE(TAG, "falha ao criar task");
        return ESP_ERR_NO_MEM;
    }

    event_bus_subscribe(EVT_TOUCH_PRESS,  on_touch_press);
    event_bus_subscribe(EVT_WAKE_WORD,    on_wake_word);

    ESP_LOGI(TAG, "ok — P%u Core1 sons=%d", TASK_PRIO, SOUND_COUNT);
    return ESP_OK;
}
