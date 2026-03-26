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

/*
 * Estados de LED de alto nível.
 * ws2812_set_state() aplica o padrão correspondente em todos os LEDs.
 * LED_STATE_CAMERA usa piscar periódico (500 ms on/off) via esp_timer.
 */
typedef enum {
    LED_STATE_NORMAL,     /* verde sólido — operação normal          */
    LED_STATE_DEGRADED,   /* âmbar sólido — safe mode / degradado    */
    LED_STATE_LISTENING,  /* vermelho sólido — escuta ativa           */
    LED_STATE_PRIVACY,    /* branco sólido — câmera/mic desativados   */
    LED_STATE_CAMERA,     /* vermelho piscante — câmera em uso        */
} led_state_t;

/*
 * Configura qual índice de LED recebe os estados de ws2812_set_state().
 * Padrão: 0 (LED onboard). Chamar antes de ws2812_set_state() se necessário.
 */
void ws2812_set_status_led(uint32_t idx);

/*
 * Define o brilho global aplicado em todos os LEDs no próximo ws2812_show().
 * 0 = apagado, 255 = brilho total. Padrão: 255.
 * Não requer re-setar os pixels — o escalonamento é aplicado na transmissão.
 */
void ws2812_set_brightness(uint8_t brightness);

/* Aplica um estado de LED no índice de status. Chame após ws2812_init(). Thread-safe. */
void ws2812_set_state(led_state_t state);

#ifdef __cplusplus
}
#endif
