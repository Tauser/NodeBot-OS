#include "brownout_handler.h"
#include "config_manager.h"
#include "log_manager.h"

#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "BROWNOUT";

/* ── Shutdown handler ──────────────────────────────────────────────── */

static void on_shutdown(void)
{
    /* Sinaliza que este boot terminou de forma não limpa */
    config_set_int("unclean_boot", 1);

    /* Incrementa contador de crashes (mantém histórico cumulativo) */
    int32_t cnt = config_get_int("crash_cnt", 0);
    config_set_int("crash_cnt", cnt + 1);

    /* Preserva o buffer de log em SD (best-effort, max ~3 ms) */
    log_flush_now();

    ESP_LOGE(TAG, "shutdown handler: unclean_boot=1  crash_cnt=%ld",
             (long)(cnt + 1));
}

/* ── Init ──────────────────────────────────────────────────────────── */

void brownout_handler_init(void)
{
    /* esp_register_shutdown_handler pode falhar se os slots estiverem cheios
     * (padrão IDF = 2). Não usar ESP_ERROR_CHECK — falha não é fatal. */
    esp_err_t err = esp_register_shutdown_handler(on_shutdown);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "shutdown handler registrado");
    } else {
        ESP_LOGW(TAG, "shutdown handler não registrado (err=0x%x) — "
                 "aumente CONFIG_ESP_SYSTEM_SHUTDOWN_HANDLERS_NO se necessário",
                 (unsigned)err);
    }
}
