#pragma once
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TelemetryService — stub opt-in.
 * Envio de telemetria desabilitado por padrão.
 * Habilitar via telemetry_service_enable(true) após consentimento do usuário.
 *
 * Quando habilitado (futuro): envia snapshot JSON periódico para endpoint
 * configurado via config_manager ("telemetry_url").
 */

esp_err_t telemetry_service_init(void);
void      telemetry_service_enable(bool enable);
bool      telemetry_service_is_enabled(void);

#ifdef __cplusplus
}
#endif
