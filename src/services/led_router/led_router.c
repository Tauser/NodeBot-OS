#include "led_router.h"

#include "event_bus.h"
#include "ws2812_driver.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "led_router";

#define LISTENING_TIMEOUT_MS  5000u

static esp_timer_handle_t s_listen_timer = NULL;

static void on_listen_timeout(void *arg)
{
    (void)arg;
    ws2812_set_state(LED_STATE_NORMAL);
}

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

static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    ws2812_set_state(LED_STATE_LISTENING);
    /* Reinicia o timer: 5s sem novo wake word → volta ao estado normal */
    esp_timer_stop(s_listen_timer);
    esp_timer_start_once(s_listen_timer, LISTENING_TIMEOUT_MS * 1000ULL);
}

esp_err_t led_router_init(void)
{
    const esp_timer_create_args_t t = {
        .callback = on_listen_timeout,
        .name     = "led_listen",
    };
    esp_timer_create(&t, &s_listen_timer);

    esp_err_t err = event_bus_subscribe(EVT_LED_CMD,   on_led_cmd);
    err |=          event_bus_subscribe(EVT_WAKE_WORD, on_wake_word);
    if (err == ESP_OK) ESP_LOGI(TAG, "OK");
    return err;
}
