#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Payload publicado nos eventos EVT_TOUCH_PRESS e EVT_TOUCH_RELEASE.
 *
 * EVT_TOUCH_PRESS  → duration_ms = 0
 * EVT_TOUCH_RELEASE → duration_ms = tempo de contato em ms
 */
typedef struct {
    uint8_t  zone_id;       /* TOUCH_ZONE_BASE / TOP / LEFT */
    float    intensity;     /* (raw − threshold) / threshold, ≥ 0.0 */
    uint32_t duration_ms;   /* preenchido no RELEASE */
} touch_event_t;

/**
 * Inicializa o TouchService.
 *
 * Chama touch_driver_init(). Tenta carregar thresholds do NVS;
 * se ausentes, executa touch_service_calibrate() automaticamente.
 * Cria a task de varredura (P7, Core 1, 20 ms).
 *
 * Deve ser chamado após config_manager_init() e event_bus_init().
 */
esp_err_t touch_service_init(void);

/**
 * Calibra todas as zonas ativas.
 *
 * Coleta 100 amostras por zona (1 amostra / 20 ms).
 * threshold = média × 150 % (ESP32-S3: valor sobe ao tocar).
 * Persiste os thresholds no NVS ao final.
 * Bloqueia ~2 s. Chamar apenas com as zonas sem toque.
 */
void touch_service_calibrate(void);

#ifdef __cplusplus
}
#endif
