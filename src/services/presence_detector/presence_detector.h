#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PresenceDetector — E42
 *
 * Detecção de presença humana e posição de rosto via câmera.
 * Dois níveis:
 *   Nível 1 — frame diff escalar: descarta frames sem movimento (< MOTION_THRESHOLD).
 *   Nível 2 — centróide ponderado de movimento: estima posição do rosto.
 *
 * Publica EVT_FACE_DETECTED (event_bus.h) apenas quando o estado muda
 * (presente→ausente ou ausente→presente).
 *
 * Roda em Core 0 como subtask de EVT_CAMERA_FRAME.
 * Uso de PSRAM: 2 × 19.200 bytes (ping-pong de frames QQVGA).
 *
 * Depende de: camera_service_init(), event_bus_init().
 */
esp_err_t presence_detector_init(void);

/*
 * Resultado de uma detecção de rosto.
 * Publicado via EVT_FACE_DETECTED no event_bus.
 */
typedef struct {
    bool    detected;    /* true = presença confirmada                  */
    float   x;          /* centro do rosto: 0.0 = esquerda, 1.0 = dir. */
    float   y;          /* centro do rosto: 0.0 = topo,   1.0 = baixo  */
    float   confidence; /* 0.0–1.0 baseado na intensidade do movimento  */
    uint16_t w;         /* largura estimada do bounding box (pixels)    */
    uint16_t h;         /* altura estimada do bounding box (pixels)     */
} face_detection_result_t;

/* Retorna o último resultado detectado (thread-safe por cópia atômica). */
face_detection_result_t presence_detector_get_last(void);

#ifdef __cplusplus
}
#endif
