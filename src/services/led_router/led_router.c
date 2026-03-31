#include "led_router.h"

#include "event_bus.h"
#include "ws2812_driver.h"
#include "esp_log.h"

static const char *TAG = "led_router";

static void on_led_cmd(uint16_t type, void *payload)
{
    (void)type;
    const led_cmd_t *cmd = (const led_cmd_t *)payload;
    if (!cmd) return;

    if (cmd->idx == LED_CMD_EMOTION) {
        ws2812_set_emotion_color(cmd->r, cmd->g, cmd->b);
    } else {
        ws2812_set_pixel(cmd->idx, cmd->r, cmd->g, cmd->b);
        ws2812_show();
    }
}

esp_err_t led_router_init(void)
{
    esp_err_t err = event_bus_subscribe(EVT_LED_CMD, on_led_cmd);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OK");
    }
    return err;
}
