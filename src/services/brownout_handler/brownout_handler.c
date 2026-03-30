#include "brownout_handler.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "brownout";

/* Namespace dedicado para contadores de safety — separado do config_manager
 * para evitar invalidar o CRC do namespace "nodebot_cfg". */
#define SAFETY_NS "nb_safety"

/* ── handler ──────────────────────────────────────────────────────────────
 * Chamado pelo ESP-IDF durante brownout ou esp_restart().
 * BUDGET: < 5ms. Apenas NVS raw. Sem SD, sem malloc, sem printf.
 * ──────────────────────────────────────────────────────────────────────── */
static void brownout_handler_fn(void)
{
    nvs_handle_t h;
    if (nvs_open(SAFETY_NS, NVS_READWRITE, &h) != ESP_OK) return;

    uint8_t count = 0;
    nvs_get_u8(h, "crash_count", &count);
    if (count < 255) count++;
    nvs_set_u8(h, "crash_count", count);
    nvs_set_u8(h, "unclean", 1);
    nvs_commit(h);
    nvs_close(h);
}

/* ── init ─────────────────────────────────────────────────────────────── */

esp_err_t brownout_handler_init(void)
{
    esp_err_t err = esp_register_shutdown_handler(brownout_handler_fn);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "handler registrado");
    } else {
        ESP_LOGE(TAG, "falha ao registrar handler: 0x%08X", (unsigned)err);
    }
    return err;
}
