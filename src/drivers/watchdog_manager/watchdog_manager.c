#include "watchdog_manager.h"

#include "esp_task_wdt.h"
#include "esp_log.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "wdt";

/* ── tabela de entradas registradas ─────────────────────────────────── */
typedef struct {
    TaskHandle_t               task;
    esp_task_wdt_user_handle_t user;
    uint32_t                   timeout_ms;
    char                       name[configMAX_TASK_NAME_LEN];
} wdt_entry_t;

static wdt_entry_t s_entries[WDT_MAX_ENTRIES];
static uint8_t     s_count;

/* ── lookup por task handle ──────────────────────────────────────────── */
static wdt_entry_t *find_entry(TaskHandle_t task)
{
    for (uint8_t i = 0; i < s_count; i++)
        if (s_entries[i].task == task) return &s_entries[i];
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════
   API pública
   ══════════════════════════════════════════════════════════════════════ */

esp_err_t wdt_init(void)
{
    s_count = 0;

    /*
     * Task WDT e HW WDT são configurados via sdkconfig.defaults antes do
     * app_main. Aqui apenas verificamos que o TWDT está ativo e logamos
     * a configuração.
     *
     * Caso o sdkconfig não o tenha habilitado, tentamos inicializá-lo.
     */
    esp_task_wdt_config_t cfg = {
        .timeout_ms     = WDT_TASK_TIMEOUT_MS,
        .idle_core_mask = 0,       /* não monitora idle tasks */
        .trigger_panic  = true,
    };

    esp_err_t ret = esp_task_wdt_reconfigure(&cfg);
    if (ret == ESP_ERR_INVALID_STATE) {
        /* TWDT não inicializado pelo sdkconfig — inicializa agora */
        ret = esp_task_wdt_init(&cfg);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "init OK  TWDT=%dms/panic  HWWDT=%dms",
                 WDT_TASK_TIMEOUT_MS, WDT_HW_TIMEOUT_MS);
    } else {
        ESP_LOGE(TAG, "TWDT init falhou: 0x%x", ret);
    }
    return ret;
}

esp_err_t wdt_register_task(TaskHandle_t task, uint32_t timeout_ms)
{
    if (s_count >= WDT_MAX_ENTRIES) {
        ESP_LOGE(TAG, "WDT_MAX_ENTRIES atingido");
        return ESP_ERR_NO_MEM;
    }

    if (!task) task = xTaskGetCurrentTaskHandle();

    const char *name = pcTaskGetName(task);
    esp_task_wdt_user_handle_t user;

    esp_err_t ret = esp_task_wdt_add_user(name, &user);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add_user '%s' falhou: 0x%x", name, ret);
        return ret;
    }

    wdt_entry_t *e = &s_entries[s_count++];
    e->task       = task;
    e->user       = user;
    e->timeout_ms = timeout_ms;
    strncpy(e->name, name, sizeof(e->name) - 1);
    e->name[sizeof(e->name) - 1] = '\0';

    ESP_LOGI(TAG, "registrado: %-16s  timeout=%" PRIu32 "ms", name, timeout_ms);
    return ESP_OK;
}

esp_err_t wdt_feed(TaskHandle_t task)
{
    if (!task) task = xTaskGetCurrentTaskHandle();

    wdt_entry_t *e = find_entry(task);
    if (!e) return ESP_ERR_NOT_FOUND;

    return esp_task_wdt_reset_user(e->user);
}
