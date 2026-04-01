#pragma once
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Inicializa o DiagnosticMode.
 * Subscreve EVT_TOUCH_PRESS — ativado por 3 toques em < 2s.
 * Quando ativo: imprime JSON de diagnóstico a cada 2s via serial.
 * Auto-desativa após 60s ou outro toque triplo.
 */
esp_err_t diagnostic_mode_init(void);

bool diagnostic_mode_is_active(void);

#ifdef __cplusplus
}
#endif
