#include "preference_memory_service.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sd_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#define TAG "prefs"

#define PREFS_NVS_NS   "nb_prefs"
#define KEY_NAME       "user_name"
#define KEY_VOLUME     "volume"
#define KEY_STYLE      "resp_style"

#define SESSION_FILE   "/sdcard/pref_sessions.bin"
#define SESSION_MAX    7

/*
 * Layout do arquivo de sessões no SD:
 *   uint8_t count      — número de entradas válidas (0..7)
 *   uint8_t head       — índice mais antigo no ring buffer
 *   session_record_t[SESSION_MAX]
 */
typedef struct __attribute__((packed)) {
    uint8_t          count;
    uint8_t          head;
    session_record_t records[SESSION_MAX];
} session_file_t;

static nvs_handle_t   s_nvs;
static char           s_name[32];
static uint8_t        s_volume;
static response_style_t s_style;
static session_file_t s_sf;

/* ── sessões ──────────────────────────────────────────────────────────── */

static void load_sessions(void)
{
    size_t n = sd_read_file(SESSION_FILE, &s_sf, sizeof(s_sf));
    if (n < sizeof(s_sf)) {
        memset(&s_sf, 0, sizeof(s_sf));
    }
    /* sanidade */
    if (s_sf.count > SESSION_MAX) s_sf.count = SESSION_MAX;
    if (s_sf.head  >= SESSION_MAX) s_sf.head  = 0;
}

static void save_sessions(void)
{
    esp_err_t err = sd_write_file(SESSION_FILE, &s_sf, sizeof(s_sf));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "save_sessions: sd_write_file falhou");
    }
}

/* ── init ─────────────────────────────────────────────────────────────── */

esp_err_t preference_memory_init(void)
{
    esp_err_t err = nvs_open(PREFS_NVS_NS, NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open '%s': %s", PREFS_NVS_NS, esp_err_to_name(err));
        return err;
    }

    size_t len = sizeof(s_name);
    if (nvs_get_str(s_nvs, KEY_NAME, s_name, &len) != ESP_OK) {
        s_name[0] = '\0';
    }

    int32_t v = 70;
    nvs_get_i32(s_nvs, KEY_VOLUME, &v);
    s_volume = (uint8_t)(v & 0xFF);

    int32_t st = RESPONSE_STYLE_NORMAL;
    nvs_get_i32(s_nvs, KEY_STYLE, &st);
    s_style = (response_style_t)st;

    load_sessions();

    ESP_LOGI(TAG, "name='%s' vol=%u style=%d sessions=%u",
             s_name, s_volume, (int)s_style, s_sf.count);
    return ESP_OK;
}

/* ── sessão ───────────────────────────────────────────────────────────── */

void preference_memory_close_session(uint32_t duration_s,
                                     uint32_t interactions,
                                     float    mood_final)
{
    session_record_t rec = {
        .timestamp_s  = (uint32_t)(esp_timer_get_time() / 1000000ULL),
        .duration_s   = duration_s,
        .interactions = interactions,
        .mood_final   = mood_final,
    };

    uint8_t slot;
    if (s_sf.count < SESSION_MAX) {
        slot = s_sf.count++;
    } else {
        slot = s_sf.head;
        s_sf.head = (s_sf.head + 1) % SESSION_MAX;
    }
    s_sf.records[slot] = rec;
    save_sessions();
}

/* ── getters / setters ────────────────────────────────────────────────── */

void preference_memory_get_name(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return;
    strncpy(buf, s_name, max_len - 1);
    buf[max_len - 1] = '\0';
}

esp_err_t preference_memory_set_name(const char *name)
{
    if (!name) return ESP_ERR_INVALID_ARG;
    strncpy(s_name, name, sizeof(s_name) - 1);
    s_name[sizeof(s_name) - 1] = '\0';
    esp_err_t err = nvs_set_str(s_nvs, KEY_NAME, s_name);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    return err;
}

uint8_t preference_memory_get_volume(void) { return s_volume; }

esp_err_t preference_memory_set_volume(uint8_t vol)
{
    if (vol == s_volume) return ESP_OK;
    s_volume = vol;
    esp_err_t err = nvs_set_i32(s_nvs, KEY_VOLUME, (int32_t)vol);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    return err;
}

response_style_t preference_memory_get_response_style(void) { return s_style; }

esp_err_t preference_memory_set_response_style(response_style_t st)
{
    if (st == s_style) return ESP_OK;
    s_style = st;
    esp_err_t err = nvs_set_i32(s_nvs, KEY_STYLE, (int32_t)st);
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    return err;
}

uint8_t preference_memory_get_sessions(session_record_t *out, uint8_t n)
{
    if (!out || n == 0 || s_sf.count == 0) return 0;
    if (n > s_sf.count) n = s_sf.count;

    /* ordem cronológica: head é o mais antigo */
    for (uint8_t i = 0; i < n; i++) {
        uint8_t idx = (s_sf.head + i) % SESSION_MAX;
        out[i] = s_sf.records[idx];
    }
    return n;
}
