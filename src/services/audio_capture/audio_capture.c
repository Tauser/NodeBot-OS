#include "audio_capture.h"
#include "vad.h"
#include "audio_driver.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include <string.h>
#include <stdint.h>

static const char *TAG = "audio_cap";

#define TASK_STACK       4096u
#define TASK_PRIO          14u

/* ── Ring buffer (PSRAM) ───────────────────────────────────────────────── */
/*
 * Layout: array circular de blocos contíguos.
 * write_idx: próximo bloco a escrever (produtor).
 * read_idx:  próximo bloco a ler (consumidor).
 * Sem mutex no write path (único produtor); leitura protegida por semáforo.
 */
#define RING_TOTAL_SAMPLES (AUDIO_CAPTURE_BLOCK_SAMPLES * AUDIO_CAPTURE_RING_BLOCKS)

static int16_t          *s_ring = NULL;
static volatile uint32_t s_write_idx = 0;  /* índice em blocos              */
static volatile uint32_t s_read_idx  = 0;  /* índice em blocos              */
static SemaphoreHandle_t  s_data_sem = NULL;

/* ── Helpers ───────────────────────────────────────────────────────────── */
static inline uint32_t blocks_available(void)
{
    uint32_t w = s_write_idx;
    uint32_t r = s_read_idx;
    return (w >= r) ? (w - r) : (AUDIO_CAPTURE_RING_BLOCKS - r + w);
}

/* ── Task de captura ───────────────────────────────────────────────────── */
static void audio_capture_task(void *arg)
{
    (void)arg;

    /* Buffer de bloco na stack (512 × 2 = 1KB — dentro do TASK_STACK) */
    int16_t block[AUDIO_CAPTURE_BLOCK_SAMPLES];

    bool last_is_speech = false;

    while (1) {
        /* Lê 512 amostras em duas chamadas de 256 (limite do driver) */
        size_t n1 = audio_mic_read(block,       256u);
        size_t n2 = audio_mic_read(block + 256u, 256u);
        size_t total = n1 + n2;

        if (total == 0) continue;

        /* VAD */
        vad_event_t vad = vad_process(block, total);

        /* Log periódico de diagnóstico — a cada ~3s (96 blocos × 32ms) */
        static uint32_t s_diag_count = 0;
        static float    s_rms_max    = 0.0f;
        {
            /* Recomputa RMS para o log (sem custo extra — bloco já em cache) */
            int64_t sq = 0;
            for (size_t i = 0; i < total; i++) { int32_t v = block[i]; sq += v*v; }
            float rms_now = sqrtf((float)sq / (float)total);
            if (rms_now > s_rms_max) s_rms_max = rms_now;
        }
        if (++s_diag_count >= 96u) {
            ESP_LOGI(TAG, "rms_peak=%.0f  vad_thr=%d  speech=%s",
                     (double)s_rms_max, 200, last_is_speech ? "ON" : "OFF");
            s_rms_max    = 0.0f;
            s_diag_count = 0u;
        }

        /* Publica EVT_VOICE_ACTIVITY apenas quando o estado muda */
        if (vad.is_speech != last_is_speech) {
            last_is_speech = vad.is_speech;
            event_bus_publish(EVT_VOICE_ACTIVITY, &vad, sizeof(vad),
                              EVENT_PRIO_BEHAVIOR);
            ESP_LOGI(TAG, "VAD %s  energy=%.1f dB",
                     vad.is_speech ? "ON " : "OFF", (double)vad.energy_db);
        }

        /* Escreve no ring buffer (sobrescreve o mais antigo se cheio) */
        uint32_t w = s_write_idx;
        memcpy(s_ring + (size_t)w * AUDIO_CAPTURE_BLOCK_SAMPLES,
               block, total * sizeof(int16_t));
        s_write_idx = (w + 1u) % AUDIO_CAPTURE_RING_BLOCKS;

        /* Sinaliza consumidores */
        xSemaphoreGive(s_data_sem);
    }
}

/* ── API pública ───────────────────────────────────────────────────────── */

size_t audio_capture_read(int16_t *buf, size_t samples)
{
    if (!s_ring || !buf || samples == 0) return 0;

    /* Aguarda pelo menos um bloco disponível */
    if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(100)) != pdTRUE) return 0;

    if (blocks_available() == 0) return 0;

    uint32_t r    = s_read_idx;
    size_t   copy = (samples > AUDIO_CAPTURE_BLOCK_SAMPLES)
                    ? AUDIO_CAPTURE_BLOCK_SAMPLES : samples;

    memcpy(buf,
           s_ring + (size_t)r * AUDIO_CAPTURE_BLOCK_SAMPLES,
           copy * sizeof(int16_t));

    s_read_idx = (r + 1u) % AUDIO_CAPTURE_RING_BLOCKS;
    return copy;
}

esp_err_t audio_capture_init(void)
{
    /* Aloca ring buffer em PSRAM (alocação única, não é hot path) */
    s_ring = heap_caps_malloc(RING_TOTAL_SAMPLES * sizeof(int16_t),
                              MALLOC_CAP_SPIRAM);
    if (!s_ring) {
        ESP_LOGE(TAG, "falha ao alocar ring buffer (%u bytes) em PSRAM",
                 (unsigned)(RING_TOTAL_SAMPLES * sizeof(int16_t)));
        return ESP_ERR_NO_MEM;
    }
    memset(s_ring, 0, RING_TOTAL_SAMPLES * sizeof(int16_t));
    s_write_idx = 0;
    s_read_idx  = 0;

    s_data_sem = xSemaphoreCreateCounting(AUDIO_CAPTURE_RING_BLOCKS, 0);
    if (!s_data_sem) {
        heap_caps_free(s_ring);
        s_ring = NULL;
        ESP_LOGE(TAG, "falha ao criar semáforo");
        return ESP_ERR_NO_MEM;
    }

    vad_init();

    BaseType_t rc = xTaskCreatePinnedToCore(
        audio_capture_task, "audio_cap",
        TASK_STACK, NULL, TASK_PRIO, NULL, 0 /* Core 0 */);

    if (rc != pdPASS) {
        vSemaphoreDelete(s_data_sem);
        heap_caps_free(s_ring);
        s_ring     = NULL;
        s_data_sem = NULL;
        ESP_LOGE(TAG, "falha ao criar task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "ok — Core0 P%u ring=%uKB blocks=%u×%u",
             TASK_PRIO,
             (unsigned)(RING_TOTAL_SAMPLES * sizeof(int16_t) / 1024u),
             AUDIO_CAPTURE_RING_BLOCKS,
             AUDIO_CAPTURE_BLOCK_SAMPLES);
    return ESP_OK;
}
