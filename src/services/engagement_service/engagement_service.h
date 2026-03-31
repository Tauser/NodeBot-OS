#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * EngagementService — monitora inatividade e sinaliza busca de atenção.
 *
 * Rastreia o timestamp da última interação. Se o intervalo ultrapassar
 * ENGAGEMENT_IDLE_MS (10 min), publica EVT_LOW_ENGAGEMENT a cada
 * ENGAGEMENT_REPEAT_MS (30 s) até que uma nova interação ocorra.
 *
 * Subscreve EVT_TOUCH_PRESS, EVT_WAKE_WORD, EVT_VOICE_ACTIVITY para
 * registrar interações e resetar o timer.
 *
 * Deve ser chamado após event_bus_init().
 */
esp_err_t engagement_service_init(void);

#ifdef __cplusplus
}
#endif
