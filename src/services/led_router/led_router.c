#include "led_router.h"

#include "event_bus.h"
#include "ws2812_driver.h"
#include "dialogue_state_service.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "led_router";

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
    /* LED vermelho acende ao entrar em LISTENING */
    ws2812_set_state(LED_STATE_LISTENING);
    /* Timer de segurança: garante reset se DialogueStateService não avisar */
    esp_timer_stop(s_listen_timer);
    esp_timer_start_once(s_listen_timer, 8000000ULL /* 8 s */);
}

static void on_dialogue_state_changed(uint16_t type, void *payload)
{
    (void)type;
    const dialogue_state_event_t *ev = (const dialogue_state_event_t *)payload;
    if (!ev) return;

    /* Desliga LED vermelho exatamente ao sair de LISTENING */
    if ((dialogue_state_t)ev->state != DIALOGUE_LISTENING) {
        esp_timer_stop(s_listen_timer);
        ws2812_set_state(LED_STATE_NORMAL);
    }
}

esp_err_t led_router_init(void)
{
    const esp_timer_create_args_t t = {
        .callback              = on_listen_timeout,
        .arg                   = NULL,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "led_listen",
        .skip_unhandled_events = false,
    };
    esp_err_t err = esp_timer_create(&t, &s_listen_timer);
    if (err != ESP_OK) return err;

    err  = event_bus_subscribe(EVT_LED_CMD,                on_led_cmd);
    err |= event_bus_subscribe(EVT_WAKE_WORD,              on_wake_word);
    err |= event_bus_subscribe(EVT_DIALOGUE_STATE_CHANGED, on_dialogue_state_changed);
    if (err == ESP_OK) ESP_LOGI(TAG, "OK");
    return err;
}
