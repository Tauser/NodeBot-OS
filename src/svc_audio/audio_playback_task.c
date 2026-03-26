#include "audio_playback_task.h"
#include "audio_feedback_priv.h"   /* s_sounds[], s_playing */
#include "audio_driver.h"
#include "vad.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "AUDIO_PB";

QueueHandle_t g_audio_cmd_queue = NULL;

static void audio_playback_task(void *arg)
{
    (void)arg;
    audio_cmd_t cmd;

    for (;;) {
        if (xQueueReceive(g_audio_cmd_queue, &cmd, portMAX_DELAY) != pdTRUE)
            continue;

        if (cmd.id >= SOUND_COUNT) continue;

        const sound_buf_t *s = &g_sounds[cmd.id];
        if (!s->samples || s->n_samples == 0) continue;

        atomic_store(&g_playing, true);

        /* Suprime VAD durante playback para evitar feedback */
        vad_suppress_ms(((uint32_t)s->n_samples * 1000u / 16000u) + 200u);

        ESP_LOGD(TAG, "play %d  n=%u", (int)cmd.id, (unsigned)s->n_samples);
        audio_play_pcm(s->samples, s->n_samples);

        atomic_store(&g_playing, false);
    }
}

void audio_playback_task_start(void)
{
    g_audio_cmd_queue = xQueueCreate(8, sizeof(audio_cmd_t));
    configASSERT(g_audio_cmd_queue);

    xTaskCreatePinnedToCore(audio_playback_task, "AudioPB",
                            4096, NULL, 18, NULL, 1);
}
