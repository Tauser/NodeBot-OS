#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa I2S1 master TX para MAX98357A: 16 kHz, 16-bit, mono. */
void max98357a_init(void);

/*
 * Envia amostras PCM para o amplificador.
 * O buffer não é modificado; volume é aplicado internamente em chunks.
 * Bloqueia até todos os dados serem enfileirados no DMA.
 */
void max98357a_play_pcm(const int16_t *buf, size_t samples);

/* Volume de software: 0 (mudo) – 100 (máximo). Padrão: 80. */
void max98357a_set_volume(uint8_t vol_pct);

/*
 * Gera uma onda senoidal em out_buf.
 *   freq_hz     : frequência em Hz
 *   duration_ms : duração em milissegundos
 *   out_buf     : buffer fornecido pelo caller (deve caber ≥ 16000 * duration_ms / 1000 amostras)
 * Retorna o número de amostras geradas.
 */
size_t generate_beep(uint16_t freq_hz, uint32_t duration_ms, int16_t *out_buf);

#ifdef __cplusplus
}
#endif
