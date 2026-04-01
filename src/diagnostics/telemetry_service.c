#include "telemetry_service.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "telemetry";

static bool s_enabled = false;

esp_err_t telemetry_service_init(void)
{
    /* Stub — envio desabilitado por padrão (opt-in).
     * Para habilitar: chamar telemetry_service_enable(true) após
     * configurar "telemetry_url" no config_manager. */
    ESP_LOGI(TAG, "stub ok — desabilitado por padrão");
    return ESP_OK;
}

void telemetry_service_enable(bool enable)
{
    s_enabled = enable;
    ESP_LOGI(TAG, "telemetria %s", enable ? "habilitada" : "desabilitada");
}

bool telemetry_service_is_enabled(void)
{
    return s_enabled;
}
