#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * touch_service — E26.
 *
 * TouchTask (Core 1, pri 7, tick 20 ms):
 *   - Lê raw por zona a cada tick.
 *   - Máquina de estado por zona: IDLE → DEBOUNCE (50 ms) → ACTIVE.
 *   - Ao confirmar toque: publica EVT_TOUCH_DETECTED.
 *
 * Thresholds (por zona):
 *   - Carregados do NVS no init.
 *   - Se ausentes: calibra automaticamente (100 amostras, × 1.5).
 *   - touch_service_calibrate() salva no NVS ao final.
 *
 * Reação básica (subscriber interno):
 *   EVT_TOUCH_DETECTED → arousal += 0.3 · emotion_mapper_apply(SURPRISED, 200ms).
 */

/* Payload de EVT_TOUCH_DETECTED */
typedef struct {
    uint8_t  zone_id;      /* 0 = base, 1 = top, 2 = left              */
    float    intensity;    /* (raw − threshold) / threshold  [0, 1]     */
    uint32_t duration_ms;  /* duração confirmada do toque (≥ debounce)  */
} touch_event_t;

/* Inicializa e cria TouchTask. Chamar após touch_driver_init() e event_bus_init(). */
void touch_service_init(void);

/*
 * Calibra todas as zonas ativas (100 amostras × 1.5 = threshold).
 * Bloqueia ~500 ms. Salva resultado no NVS.
 * Chamado automaticamente pelo init se os thresholds não existirem.
 */
void touch_service_calibrate(void);

#ifdef __cplusplus
}
#endif
