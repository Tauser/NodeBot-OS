#include "audio_capture.h"
#include "audio_driver.h"
#include "vad.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "AUDIO_CAP";

/* ── Ring buffer em PSRAM ──────────────────────────────────────────── */
static int16_t (*s_ring)[AUDIO_BLOCK_SAMPLES] = NULL;  /* [AUDIO_RING_BLOCKS][AUDIO_BLOCK_SAMPLES] */
static volatile uint32_t s_write_idx   = 0;   /* próximo bloco a escrever       */
static volatile uint32_t s_block_count = 0;   /* blocos escritos (satura)       */

static SemaphoreHandle_t s_mutex = NULL;

/* ── Task de captura ───────────────────────────────────────────────── */

static void audio_capture_task(void *arg)
{
    (void)arg;

    int16_t tmp[AUDIO_BLOCK_SAMPLES];

    /* DEBUG stack — remover após confirmar */
    ESP_LOGI(TAG, "stack HWM inicial: %u words livres",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));

    for (;;) {
        /* audio_mic_read lê no máx 256 amostras por chamada */
        size_t got = audio_mic_read(tmp, AUDIO_BLOCK_SAMPLES);
        if (got == 0) continue;

        /* Copia para o slot atual do ring buffer */
        uint32_t slot;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        slot = s_write_idx;
        s_write_idx = (s_write_idx + 1u) % AUDIO_RING_BLOCKS;
        if (s_block_count < AUDIO_RING_BLOCKS) s_block_count++;
        xSemaphoreGive(s_mutex);

        memcpy(s_ring[slot], tmp, got * sizeof(int16_t));

        /* Passa apenas as amostras reais para o VAD */
        vad_process_block(tmp, got);
    }
}

/* ── Init ──────────────────────────────────────────────────────────── */

void audio_capture_init(void)
{
    /* Aloca ring buffer em PSRAM */
    const size_t total = AUDIO_RING_BLOCKS * AUDIO_BLOCK_SAMPLES * sizeof(int16_t);
    s_ring = heap_caps_malloc(total, MALLOC_CAP_SPIRAM);
    if (!s_ring) {
        ESP_LOGE(TAG, "falha ao alocar ring buffer (%u bytes) em PSRAM", (unsigned)total);
        return;
    }
    memset(s_ring, 0, total);

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "falha ao criar mutex");
        return;
    }

    xTaskCreatePinnedToCore(audio_capture_task, "AudioCap",
                            6144, NULL, 14, NULL, 0);

    ESP_LOGI(TAG,
             "AudioCaptureTask Core 0 pri 14  ring=%u blocos  psram=%u bytes",
             (unsigned)AUDIO_RING_BLOCKS, (unsigned)total);
}

/* ── API pública ───────────────────────────────────────────────────── */

bool audio_capture_get_latest(int16_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool valid = (s_block_count > 0);
    uint32_t idx = 0;
    if (valid) {
        idx = (s_write_idx == 0) ? (AUDIO_RING_BLOCKS - 1u)
                                 : (s_write_idx - 1u);
    }
    xSemaphoreGive(s_mutex);

    if (valid) {
        memcpy(out, s_ring[idx], AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
    }
    return valid;
}
