#pragma once

#include <stdint.h>
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Estados de sistema do LED de status (LED 0) ─────────────────────────
 * Definidos por E25. Não desabilitáveis por código de alto nível.
 * Qualquer chamada a ws2812_show() re-aplica o estado atual sobre LED 0.
 * ──────────────────────────────────────────────────────────────────────── */
typedef enum {
    LED_STATE_NORMAL    = 0,  /* verde          — operação normal          */
    LED_STATE_DEGRADED  = 1,  /* âmbar          — safe mode / degradado    */
    LED_STATE_LISTENING = 2,  /* vermelho fixo  — mic ativo                */
    LED_STATE_CAMERA    = 3,  /* vermelho piscante — câmera ativa           */
    LED_STATE_PRIVACY   = 4,  /* branco fixo    — privacy mode             */
} led_state_t;

/* Inicializa o canal RMT para a cadeia de `num_leds` WS2812B no `gpio`. */
void ws2812_init(gpio_num_t gpio, uint32_t num_leds);

/* Define a cor do LED `idx` (0-based). Não atualiza a fita ainda.
 * NOTA: LED 0 é sempre sobrescrito pelo estado de sistema em ws2812_show(). */
void ws2812_set_pixel(uint32_t idx, uint8_t r, uint8_t g, uint8_t b);

/* Transmite o buffer de cores para a fita via RMT.
 * Sempre aplica o estado de sistema sobre LED 0 antes de transmitir. */
void ws2812_show(void);

/* Define o estado de sistema (LED 0). Aplica imediatamente.
 * LED_STATE_CAMERA inicia blink automático a 2 Hz via esp_timer.
 * Única forma de sobrescrever: chamar novamente ws2812_set_state(). */
void ws2812_set_state(led_state_t state);

/* Retorna o estado de sistema atual. */
led_state_t ws2812_get_state(void);

#ifdef __cplusplus
}
#endif
