#pragma once

#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa o canal RMT para a cadeia de `num_leds` WS2812B no `gpio`. */
void ws2812_init(gpio_num_t gpio, uint32_t num_leds);

/* Define a cor do LED `idx` (0-based). Não atualiza a fita ainda. */
void ws2812_set_pixel(uint32_t idx, uint8_t r, uint8_t g, uint8_t b);

/* Transmite o buffer de cores para a fita via RMT. */
void ws2812_show(void);

#ifdef __cplusplus
}
#endif
