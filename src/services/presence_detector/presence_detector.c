#include "presence_detector.h"
#include "camera_service.h"
#include "event_bus.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "presence";

/* ── Dimensões do frame QQVGA ─────────────────────────────────────────────── */
#define FRAME_W    160u
#define FRAME_H    120u
#define FRAME_SIZE (FRAME_W * FRAME_H)   /* 19.200 bytes */

/* ── Parâmetros de detecção ───────────────────────────────────────────────── */
#define MOTION_THRESHOLD     8u    /* diff médio mínimo para processar (Nível 1) */
#define PRESENCE_THRESHOLD  15u    /* diff médio para confirmar presença          */
#define ABSENCE_THRESHOLD    6u    /* diff médio para confirmar ausência          */
#define CONFIRM_FRAMES       3u    /* frames consecutivos para confirmar entrada  */
#define ABSENCE_FRAMES       6u    /* frames consecutivos para confirmar saída    */
#define CONFIDENCE_SCALE    50.0f  /* diff=50 → confidence=1.0                   */

/* Bounding box estimado: ~25% da dimensão (rosto típico a ~1 m em QQVGA) */
#define BB_W_DEFAULT  40u
#define BB_H_DEFAULT  40u

/* ── Estado ───────────────────────────────────────────────────────────────── */

/* Buffers ping-pong em PSRAM */
static uint8_t *s_buf_curr = NULL;
static uint8_t *s_buf_prev = NULL;

/* Fila: ponteiro para o frame copiado mais recente (overwrite) */
static QueueHandle_t s_queue = NULL;

/* Estado de presença com histerese */
static bool     s_present        = false;
static uint8_t  s_confirm_count  = 0u;
static uint8_t  s_absence_count  = 0u;

/* Último resultado — copiado atomicamente (tarefa única escreve) */
static face_detection_result_t s_last = {0};

/* ── Algoritmo de detecção ────────────────────────────────────────────────── */

/*
 * Nível 2: centróide ponderado de movimento.
 * Para cada pixel, o peso é a diferença absoluta entre curr e prev.
 * O centróide ponderado fornece a posição estimada do rosto em [0,1].
 */
static void compute_centroid(const uint8_t *curr, const uint8_t *prev,
                              float *out_x, float *out_y, float *out_conf)
{
    uint64_t sum_w  = 0u;
    uint64_t sum_wx = 0u;
    uint64_t sum_wy = 0u;

    for (uint32_t y = 0u; y < FRAME_H; y++) {
        for (uint32_t x = 0u; x < FRAME_W; x++) {
            uint32_t idx = y * FRAME_W + x;
            uint8_t  d   = (uint8_t)abs((int)curr[idx] - (int)prev[idx]);
            sum_w  += d;
            sum_wx += (uint64_t)d * x;
            sum_wy += (uint64_t)d * y;
        }
    }

    if (sum_w == 0u) {
        *out_x    = 0.5f;
        *out_y    = 0.5f;
        *out_conf = 0.0f;
        return;
    }

    *out_x = (float)sum_wx / (float)sum_w / (float)(FRAME_W - 1u);
    *out_y = (float)sum_wy / (float)sum_w / (float)(FRAME_H - 1u);

    /* Normaliza confiança: diff médio / escala, saturado em 1.0 */
    float avg_diff = (float)sum_w / (float)FRAME_SIZE;
    *out_conf = avg_diff / CONFIDENCE_SCALE;
    if (*out_conf > 1.0f) *out_conf = 1.0f;
}

/* ── Task de detecção ─────────────────────────────────────────────────────── */

static void detector_task(void *arg)
{
    (void)arg;
    uint8_t *frame_ptr;

    while (true) {
        if (xQueueReceive(s_queue, &frame_ptr, portMAX_DELAY) != pdTRUE) continue;

        /* Nível 1: diff escalar rápido */
        uint8_t diff_score = camera_service_get_frame_diff(s_buf_prev, frame_ptr);

        if (diff_score < MOTION_THRESHOLD) {
            /* Sem movimento — conta frames de ausência se estava presente */
            if (s_present) {
                s_absence_count++;
                s_confirm_count = 0u;
                if (s_absence_count >= ABSENCE_FRAMES) {
                    s_present       = false;
                    s_absence_count = 0u;

                    face_detection_result_t ev = {
                        .detected   = false,
                        .x          = s_last.x,
                        .y          = s_last.y,
                        .confidence = 0.0f,
                        .w          = 0u,
                        .h          = 0u,
                    };
                    s_last = ev;
                    event_bus_publish(EVT_FACE_DETECTED, &ev, sizeof(ev),
                                      EVENT_PRIO_BEHAVIOR);
                    ESP_LOGI(TAG, "AUSENTE");
                }
            }
            /* Atualiza prev mesmo sem movimento (drift lento) */
            memcpy(s_buf_prev, frame_ptr, FRAME_SIZE);
            continue;
        }

        /* Nível 2: centróide ponderado de movimento */
        float cx, cy, conf;
        compute_centroid(frame_ptr, s_buf_prev, &cx, &cy, &conf);

        memcpy(s_buf_prev, frame_ptr, FRAME_SIZE);

        /* Histerese de entrada */
        if (!s_present) {
            if (diff_score >= PRESENCE_THRESHOLD) {
                s_confirm_count++;
                s_absence_count = 0u;
                if (s_confirm_count >= CONFIRM_FRAMES) {
                    s_present       = true;
                    s_confirm_count = 0u;

                    face_detection_result_t ev = {
                        .detected   = true,
                        .x          = cx,
                        .y          = cy,
                        .confidence = conf,
                        .w          = BB_W_DEFAULT,
                        .h          = BB_H_DEFAULT,
                    };
                    s_last = ev;
                    event_bus_publish(EVT_FACE_DETECTED, &ev, sizeof(ev),
                                      EVENT_PRIO_BEHAVIOR);
                    ESP_LOGI(TAG, "PRESENTE x=%.2f y=%.2f conf=%.2f",
                             (double)cx, (double)cy, (double)conf);
                }
            } else {
                /* diff entre thresholds: não confirma, reseta contador */
                s_confirm_count = 0u;
            }
        } else {
            /* Já presente: atualiza posição a cada frame com movimento */
            s_absence_count = 0u;

            face_detection_result_t upd = {
                .detected   = true,
                .x          = cx,
                .y          = cy,
                .confidence = conf,
                .w          = BB_W_DEFAULT,
                .h          = BB_H_DEFAULT,
            };
            s_last = upd;
            /* Não re-publica a cada frame — apenas quando muda de estado.
             * BehaviorEngine usa presence_detector_get_last() para polling. */
        }
    }
}

/* ── Callback EVT_CAMERA_FRAME ───────────────────────────────────────────── */

static void on_camera_frame(uint16_t type, void *payload)
{
    (void)type;
    if (!payload) return;

    const camera_frame_event_t *ev = (const camera_frame_event_t *)payload;
    if (!ev->frame_buf) return;

    /* Copia frame para buffer curr antes que o camera_service possa reutilizá-lo */
    memcpy(s_buf_curr, ev->frame_buf, FRAME_SIZE);

    /* Envia ponteiro para a task (overwrite: mantém sempre o frame mais recente) */
    xQueueOverwrite(s_queue, &s_buf_curr);
}

/* ── API pública ──────────────────────────────────────────────────────────── */

face_detection_result_t presence_detector_get_last(void)
{
    return s_last;  /* cópia por valor — seguro sem mutex (struct pequena) */
}

esp_err_t presence_detector_init(void)
{
    /* Aloca buffers em PSRAM */
    s_buf_curr = heap_caps_malloc(FRAME_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_buf_prev = heap_caps_malloc(FRAME_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buf_curr || !s_buf_prev) {
        ESP_LOGE(TAG, "falha ao alocar buffers de frame em PSRAM");
        heap_caps_free(s_buf_curr);
        heap_caps_free(s_buf_prev);
        return ESP_ERR_NO_MEM;
    }
    memset(s_buf_curr, 0, FRAME_SIZE);
    memset(s_buf_prev, 0, FRAME_SIZE);

    s_queue = xQueueCreate(1, sizeof(uint8_t *));
    if (!s_queue) {
        ESP_LOGE(TAG, "falha ao criar queue");
        return ESP_ERR_NO_MEM;
    }

    event_bus_subscribe(EVT_CAMERA_FRAME, on_camera_frame);

    /* Task no Core 0 para não competir com Face/Behavior no Core 1 */
    BaseType_t ret = xTaskCreatePinnedToCore(
        detector_task, "presence", 4096, NULL, 11, NULL, 0
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "falha ao criar detector_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ok — aguardando EVT_CAMERA_FRAME (diff thr=%u/%u)",
             MOTION_THRESHOLD, PRESENCE_THRESHOLD);
    return ESP_OK;
}
