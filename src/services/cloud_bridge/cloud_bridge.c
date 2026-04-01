#include "cloud_bridge.h"
#include "wifi_manager.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_crc.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "cloud";

/* ── Configuração ──────────────────────────────────────────────────────── */
#define NVS_NS              "wb_cloud"
#define TASK_STACK           6144u
#define TASK_PRIO               8u
#define TASK_CORE               0
#define QUEUE_DEPTH             1u

#define T_HARD_MS            1200u
#define T_SOFT_MS             600u

#define WHISPER_URL  "https://api.openai.com/v1/audio/transcriptions"
#define BOUNDARY     "NbBnd42"
#define MAX_SAMPLES  (16000u * 3u)   /* 3 s de áudio */
#define RESP_BUF     512u

/* ── Partes multipart (literais estáticos — sem malloc) ─────────────────── */
static const char k_p_model[] =
    "--" BOUNDARY "\r\n"
    "Content-Disposition: form-data; name=\"model\"\r\n"
    "\r\n"
    "whisper-1\r\n";

static const char k_p_lang[] =
    "--" BOUNDARY "\r\n"
    "Content-Disposition: form-data; name=\"language\"\r\n"
    "\r\n"
    "pt\r\n";

static const char k_p_file[] =
    "--" BOUNDARY "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
    "Content-Type: audio/wav\r\n"
    "\r\n";

static const char k_p_end[] = "\r\n--" BOUNDARY "--\r\n";

/* ── Estado global ─────────────────────────────────────────────────────── */
typedef struct {
    size_t          samples;
    cloud_stt_cb_t  cb;
    uint32_t        submit_ms;
} cloud_req_t;

typedef struct {
    uint32_t crc;
    char     text[60];
    bool     valid;
} cache_t;

static QueueHandle_t   s_queue     = NULL;
static int16_t        *s_audio_buf = NULL;   /* PSRAM — único slot */
static volatile bool   s_busy      = false;
static cache_t         s_cache     = {0};
static char            s_api_key[60] = {0};
static bool            s_inited    = false;

/* ── Helpers ───────────────────────────────────────────────────────────── */

static void build_wav_header(uint8_t *h, uint32_t n_samples)
{
    uint32_t data_bytes = n_samples * 2u;
    uint32_t file_size  = data_bytes + 36u;
    uint32_t byte_rate  = 32000u;
    uint16_t v;

    memcpy(h,    "RIFF", 4); memcpy(h+4,  &file_size,  4);
    memcpy(h+8,  "WAVE", 4); memcpy(h+12, "fmt ",      4);
    uint32_t fmt_sz = 16; memcpy(h+16, &fmt_sz, 4);
    v=1;  memcpy(h+20, &v, 2);              /* PCM        */
    v=1;  memcpy(h+22, &v, 2);              /* mono       */
    uint32_t sr=16000; memcpy(h+24, &sr, 4);
    memcpy(h+28, &byte_rate, 4);
    v=2;  memcpy(h+32, &v, 2);              /* block align */
    v=16; memcpy(h+34, &v, 2);              /* bits        */
    memcpy(h+36, "data", 4); memcpy(h+40, &data_bytes, 4);
}

static bool http_write_all(esp_http_client_handle_t c, const void *buf, size_t len)
{
    const char *p = (const char *)buf;
    while (len > 0) {
        int w = esp_http_client_write(c, p, (int)(len > 4096u ? 4096u : len));
        if (w < 0) return false;
        p += w; len -= (size_t)w;
    }
    return true;
}

static bool parse_text(const char *json, char *out, size_t out_sz)
{
    const char *p = strstr(json, "\"text\":");
    if (!p) return false;
    p += 7;
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return false;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t len = (size_t)(end - p);
    if (len == 0 || len >= out_sz) return false;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000LL);
}

/* ── CloudTask ─────────────────────────────────────────────────────────── */

static void cloud_task(void *arg)
{
    cloud_req_t req;
    char resp[RESP_BUF];
    char text[60];
    uint8_t wav_hdr[44];

    while (true) {
        xQueueReceive(s_queue, &req, portMAX_DELAY);

        uint32_t elapsed = now_ms() - req.submit_ms;
        if (elapsed >= T_HARD_MS) {
            ESP_LOGW(TAG, "cloud_timeout_dropped (pre-send %"PRIu32"ms)", elapsed);
            if (req.cb) req.cb(NULL);
            s_busy = false;
            continue;
        }

        /* Cache hit — CRC32 dos primeiros 100ms de áudio */
        uint32_t crc = esp_rom_crc32_le(0, (const uint8_t *)s_audio_buf,
                                          (req.samples < 1600u ? req.samples : 1600u)
                                          * sizeof(int16_t));
        if (s_cache.valid && s_cache.crc == crc) {
            ESP_LOGI(TAG, "cache hit: '%s'", s_cache.text);
            if (req.cb) req.cb(s_cache.text);
            s_busy = false;
            continue;
        }

        if (!wifi_manager_is_connected()) {
            if (req.cb) req.cb(NULL);
            s_busy = false;
            continue;
        }

        /* Monta cabeçalho de autorização */
        char auth[80];
        snprintf(auth, sizeof(auth), "Bearer %s", s_api_key);

        /* Calcula Content-Length */
        build_wav_header(wav_hdr, (uint32_t)req.samples);
        size_t wav_bytes    = 44u + req.samples * 2u;
        int64_t content_len = (int64_t)(sizeof(k_p_model) - 1u) +
                              (int64_t)(sizeof(k_p_lang)  - 1u) +
                              (int64_t)(sizeof(k_p_file)  - 1u) +
                              (int64_t)wav_bytes             +
                              (int64_t)(sizeof(k_p_end) - 1u);

        uint32_t timeout = T_HARD_MS - elapsed;
        esp_http_client_config_t cfg = {
            .url               = WHISPER_URL,
            .method            = HTTP_METHOD_POST,
            .timeout_ms        = (int)timeout,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .buffer_size       = 512,
            .buffer_size_tx    = 512,
        };

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        char ct[64];
        snprintf(ct, sizeof(ct), "multipart/form-data; boundary=" BOUNDARY);
        esp_http_client_set_header(client, "Authorization",  auth);
        esp_http_client_set_header(client, "Content-Type",   ct);

        bool transcript_ok = false;

        if (esp_http_client_open(client, content_len) == ESP_OK) {
            bool sent = http_write_all(client, k_p_model,    sizeof(k_p_model) - 1u) &&
                        http_write_all(client, k_p_lang,     sizeof(k_p_lang)  - 1u) &&
                        http_write_all(client, k_p_file,     sizeof(k_p_file)  - 1u) &&
                        http_write_all(client, wav_hdr,      44u)                     &&
                        http_write_all(client, s_audio_buf,  req.samples * 2u)        &&
                        http_write_all(client, k_p_end,      sizeof(k_p_end) - 1u);

            if (sent) {
                esp_http_client_fetch_headers(client);
                int status = esp_http_client_get_status_code(client);

                if (status == 200) {
                    memset(resp, 0, sizeof(resp));
                    esp_http_client_read(client, resp, (int)(sizeof(resp) - 1u));
                    transcript_ok = parse_text(resp, text, sizeof(text));
                } else {
                    ESP_LOGW(TAG, "Whisper HTTP %d", status);
                }
            }
        }

        esp_http_client_cleanup(client);

        elapsed = now_ms() - req.submit_ms;
        if (elapsed >= T_HARD_MS) {
            ESP_LOGW(TAG, "cloud_timeout_dropped (post-recv %"PRIu32"ms)", elapsed);
            if (req.cb) req.cb(NULL);
        } else if (transcript_ok) {
            if (elapsed >= T_SOFT_MS) {
                ESP_LOGD(TAG, "resposta tardia (%"PRIu32"ms > T_soft)", elapsed);
            }
            ESP_LOGI(TAG, "stt='%s' %"PRIu32"ms", text, elapsed);
            s_cache.crc   = crc;
            s_cache.valid = true;
            strncpy(s_cache.text, text, sizeof(s_cache.text) - 1u);
            if (req.cb) req.cb(s_cache.text);
        } else {
            if (req.cb) req.cb(NULL);
        }

        s_busy = false;
    }
}

/* ── API pública ───────────────────────────────────────────────────────── */

esp_err_t cloud_bridge_init(void)
{
    s_audio_buf = heap_caps_malloc(MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!s_audio_buf) {
        ESP_LOGE(TAG, "sem PSRAM para audio buffer");
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(s_api_key);
        nvs_get_str(nvs, "openai_key", s_api_key, &len);
        nvs_close(nvs);
    }

    if (s_api_key[0] == '\0') {
        ESP_LOGW(TAG, "OpenAI key ausente — cloud inativo (use cloud_bridge_set_api_key)");
    }

    s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(cloud_req_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    BaseType_t ret = xTaskCreatePinnedToCore(cloud_task, "cloud_task",
                                             TASK_STACK, NULL, TASK_PRIO,
                                             NULL, TASK_CORE);
    if (ret != pdPASS) return ESP_FAIL;

    s_inited = true;
    ESP_LOGI(TAG, "ok — key=%s PSRAM=%uKB",
             s_api_key[0] ? "ok" : "ausente",
             (unsigned)(MAX_SAMPLES * sizeof(int16_t) / 1024u));
    return ESP_OK;
}

esp_err_t cloud_bridge_set_api_key(const char *key)
{
    if (!key || strlen(key) < 10u) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) return ret;
    nvs_set_str(nvs, "openai_key", key);
    nvs_commit(nvs);
    nvs_close(nvs);
    strncpy(s_api_key, key, sizeof(s_api_key) - 1u);
    ESP_LOGI(TAG, "API key configurada");
    return ESP_OK;
}

bool cloud_bridge_is_available(void)
{
    return s_inited && wifi_manager_is_connected() && s_api_key[0] != '\0';
}

void cloud_bridge_request_stt(const int16_t *audio, size_t samples,
                               cloud_stt_cb_t cb)
{
    if (!s_inited || s_busy || !wifi_manager_is_connected() || s_api_key[0] == '\0') {
        if (cb) cb(NULL);
        return;
    }

    size_t n = samples < MAX_SAMPLES ? samples : MAX_SAMPLES;
    memcpy(s_audio_buf, audio, n * sizeof(int16_t));

    cloud_req_t req = {
        .samples   = n,
        .cb        = cb,
        .submit_ms = now_ms(),
    };

    s_busy = true;
    if (xQueueSend(s_queue, &req, 0) != pdTRUE) {
        s_busy = false;
        if (cb) cb(NULL);
    }
}
