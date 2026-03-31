#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa o LED Router.
 * Assina EVT_LED_CMD no EventBus e roteia para ws2812_driver:
 *   idx == LED_CMD_EMOTION (0xFF) → ws2812_set_emotion_color(r, g, b)
 *   idx == 0..N             → ws2812_set_pixel(idx, r, g, b) + ws2812_show()
 *
 * Deve ser chamado após event_bus_init() e ws2812_init().
 */
esp_err_t led_router_init(void);

#ifdef __cplusplus
}
#endif
