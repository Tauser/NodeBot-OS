#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Payloads de evento ─────────────────────────────────────────────── */

/* Publicado em EVT_CAMERA_FRAME após cada captura bem-sucedida */
typedef struct {
    const uint8_t *frame_buf;   /* grayscale QQVGA 160×120 em PSRAM (estático) */
    uint16_t       width;       /* 160 */
    uint16_t       height;      /* 120 */
    uint32_t       timestamp_ms;
} camera_frame_event_t;

/* Publicado em EVT_PRESENCE_DETECTED quando tier muda (EMPTY↔OCCUPIED) */
typedef struct {
    bool    present;      /* true = pessoa detectada             */
    uint8_t confidence;   /* 0..100 baseado no diff normalizado  */
} presence_event_t;

/*
 * Inicializa o CameraService.
 *
 * Aloca dois frame buffers grayscale QQVGA em PSRAM.
 * Configura OV2640: grayscale, FRAMESIZE_QQVGA, fb_count=1,
 *   CAMERA_GRAB_WHEN_EMPTY, xclk=10 MHz.
 * Cria CameraCaptureTask (Core 0, P12).
 *
 * IMPORTANTE: chamar após i2c_master_init() e event_bus_init().
 * O I2C não pode ser acessado enquanto uma captura está em andamento.
 */
esp_err_t camera_service_init(void);

/*
 * Enfileira uma requisição de captura — retorna imediatamente.
 * CameraCaptureTask captura no próximo ciclo e publica EVT_CAMERA_FRAME.
 * Se já houver uma captura pendente, a requisição é ignorada.
 */
void camera_service_request_frame(void);

/* Retorna true se a câmera foi inicializada com sucesso. */
bool camera_service_is_ready(void);

/*
 * Calcula a diferença média absoluta entre dois frames grayscale QQVGA.
 * Usa amostragem a cada 4 pixels (stride) para reduzir carga de CPU.
 * Retorna valor em [0, 255].
 */
uint8_t camera_service_get_frame_diff(const uint8_t *prev, const uint8_t *curr);

#ifdef __cplusplus
}
#endif
