#pragma once

#include <stdint.h>

/*
 * Wrapper C-friendly sobre LovyanGFX + ST7789.
 * Implementado em display.cpp (C++); seguro para incluir em C e C++.
 *
 * Sprites: o caller cria lgfx::LGFX_Sprite e passa como void *.
 * Em C++, cast implícito funciona. Em C, use (void *).
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa SPI, painel e limpa a tela (preto). */
void display_init(void);

/* Preenche toda a tela com a cor em formato RGB565. */
void display_fill_color(uint16_t color);

/* Desenha retângulo preenchido. */
void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/*
 * Empurra sprite para o display na posição (x, y).
 * sprite: ponteiro para lgfx::LGFX_Sprite criado pelo caller.
 * O sprite deve ter sido inicializado com psram=true para buffers grandes.
 */
void display_push_sprite(void *sprite, int16_t x, int16_t y);

/*
 * Teste de FPS: preenche a tela alternando duas cores por duration_ms.
 * Imprime resultado via ESP_LOGI. Retorna frames por segundo medidos.
 */
float display_fps_test(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
