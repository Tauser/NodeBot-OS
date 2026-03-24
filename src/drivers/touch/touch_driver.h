#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "hal_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * IDs de zona — mapeados para HAL_TOUCH_ZONE_* em hal_init.h.
 * Usar apenas zonas < HAL_TOUCH_ZONE_COUNT.
 */
typedef enum {
    TOUCH_ZONE_BASE  = 0,
    TOUCH_ZONE_TOP   = 1,
    TOUCH_ZONE_LEFT  = 2,
    /* TOUCH_ZONE_RIGHT = 3 — pendente GPIO */
} touch_zone_t;

/* Inicializa o periférico touch e configura as zonas ativas. */
void touch_driver_init(void);

/*
 * Calibra todas as zonas: lê 100 amostras por zona,
 * calcula threshold = média × 0.75.
 * Bloqueia por ~500 ms no total.
 */
void touch_driver_calibrate(void);

/* Lê o valor raw da zona (valor bruto do periférico). */
uint32_t touch_driver_read_raw(touch_zone_t zone);

/* Retorna true se a zona estiver sendo tocada (raw < threshold). */
bool touch_driver_is_touched(touch_zone_t zone);

#ifdef __cplusplus
}
#endif
