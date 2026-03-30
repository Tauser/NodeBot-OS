#include "safe_mode_service.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "safe_mode";

#define SAFETY_NS        "nb_safety"
#define BOOT_COUNT_MAX   3                          /* safe mode na 3ª falha */
#define STABLE_TIMEOUT_US (60ULL * 1000000ULL)      /* 60 s */

static bool              s_active = false;
static esp_timer_handle_t s_stable_timer = NULL;

/* ── safe_mode_check ──────────────────────────────────────────────────── */

esp_err_t safe_mode_check(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SAFETY_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open falhou: 0x%08X", (unsigned)err);
        return err;
    }

    /* Verifica encerramento sujo do boot anterior */
    uint8_t unclean = 0;
    nvs_get_u8(h, "unclean", &unclean);
    if (unclean) {
        uint8_t crashes = 0;
        nvs_get_u8(h, "crash_count", &crashes);
        ESP_LOGW(TAG, "boot anterior encerrou de forma anormal (crash_count=%d)", crashes);
    }
    /* Limpa flag — brownout_handler seta novamente se necessário */
    nvs_set_u8(h, "unclean", 0);

    /* Incrementa boot_count */
    uint8_t count = 0;
    nvs_get_u8(h, "boot_count", &count);
    if (count < 255) count++;
    nvs_set_u8(h, "boot_count", count);
    nvs_commit(h);
    nvs_close(h);

    ESP_LOGI(TAG, "boot_count=%d (safe mode em >=%d)", count, BOOT_COUNT_MAX);

    if (count >= BOOT_COUNT_MAX) {
        s_active = true;
        ESP_LOGE(TAG, "═══════════════════════════════════════");
        ESP_LOGE(TAG, "  SAFE MODE ATIVO — boot_count=%d", count);
        ESP_LOGE(TAG, "  Diagnostico serial habilitado.");
        ESP_LOGE(TAG, "═══════════════════════════════════════");
    }

    return ESP_OK;
}

/* ── safe_mode_is_active ─────────────────────────────────────────────── */

bool safe_mode_is_active(void)
{
    return s_active;
}

/* ── stable timer ─────────────────────────────────────────────────────── */

static void stable_timer_cb(void *arg)
{
    (void)arg;
    nvs_handle_t h;
    if (nvs_open(SAFETY_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "boot_count", 0);
        nvs_set_u8(h, "crash_count", 0);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "sistema estavel por 60s — contadores zerados");
}

esp_err_t safe_mode_start_stable_timer(void)
{
    if (s_stable_timer) return ESP_OK;   /* já agendado */

    esp_timer_create_args_t args = {
        .callback              = stable_timer_cb,
        .arg                   = NULL,
        .name                  = "safe_stable",
        .skip_unhandled_events = true,
    };
    esp_err_t err = esp_timer_create(&args, &s_stable_timer);
    if (err != ESP_OK) return err;

    return esp_timer_start_once(s_stable_timer, STABLE_TIMEOUT_US);
}
