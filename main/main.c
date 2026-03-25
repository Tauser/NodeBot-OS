#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "boot_sequence.h"
#include "emotion_mapper.h"

static const char *TAG = "main";

void app_main(void)
{
    app_boot();

    /* ── Homologação: expressão fixa para observar gaze/blink isolados ── */
    emotion_mapper_apply(EMOTION_NEUTRAL, 0);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
