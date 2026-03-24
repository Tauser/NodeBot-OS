#include "touch_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "esp_log.h"

/* ─── mapeamento zona → touch pad ───────────────────────────────── */

static const touch_pad_t k_pads[HAL_TOUCH_ZONE_COUNT] = {
    HAL_TOUCH_ZONE_BASE,            /* zone 0 — GPIO2 */
#if HAL_TOUCH_ZONE_COUNT > 1
    HAL_TOUCH_ZONE_TOP,             /* zone 1 — GPIO1 */
#endif
#if HAL_TOUCH_ZONE_COUNT > 2
    HAL_TOUCH_ZONE_LEFT,            /* zone 2 — GPIO14 */
#endif
};

static const char *k_names[HAL_TOUCH_ZONE_COUNT] = {
    "base",
#if HAL_TOUCH_ZONE_COUNT > 1
    "top",
#endif
#if HAL_TOUCH_ZONE_COUNT > 2
    "left",
#endif
};

/* ─── estado ──────────────────────────────────────────────────────── */

static const char *TAG = "touch";
static uint32_t    s_threshold[HAL_TOUCH_ZONE_COUNT];

/* ─────────────────────────────────────────────────────────────────── */

void touch_driver_init(void)
{
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);

    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        ESP_ERROR_CHECK(touch_pad_config(k_pads[z]));
    }

    touch_pad_fsm_start();
    vTaskDelay(pdMS_TO_TICKS(50)); /* aguarda primeira medição */

    ESP_LOGI(TAG, "inicializado — %d zonas", HAL_TOUCH_ZONE_COUNT);
}

/* ─────────────────────────────────────────────────────────────────── */

void touch_driver_calibrate(void)
{
#define CAL_SAMPLES 100

    ESP_LOGI(TAG, "calibrando — mantenha as zonas sem toque...");

    for (int z = 0; z < HAL_TOUCH_ZONE_COUNT; z++) {
        uint64_t sum = 0;

        for (int i = 0; i < CAL_SAMPLES; i++) {
            uint32_t raw;
            touch_pad_read_raw_data(k_pads[z], &raw);
            sum += raw;
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        uint32_t mean = (uint32_t)(sum / CAL_SAMPLES);
        s_threshold[z] = mean * 150 / 100; /* 150 % da média — S3: valor SOBE ao tocar */

        ESP_LOGI(TAG, "zona %-5s  média=%"PRIu32"  threshold=%"PRIu32,
                 k_names[z], mean, s_threshold[z]);
    }

    ESP_LOGI(TAG, "calibração concluída");
}

/* ─────────────────────────────────────────────────────────────────── */

uint32_t touch_driver_read_raw(touch_zone_t zone)
{
    if (zone >= HAL_TOUCH_ZONE_COUNT) return 0;
    uint32_t val = 0;
    touch_pad_read_raw_data(k_pads[zone], &val);
    return val;
}

bool touch_driver_is_touched(touch_zone_t zone)
{
    if (zone >= HAL_TOUCH_ZONE_COUNT) return false;
    return touch_driver_read_raw(zone) > s_threshold[zone];
}
