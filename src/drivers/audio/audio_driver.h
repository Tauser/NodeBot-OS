#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * audio_driver — I2S0 full-duplex unificado.
 *
 * INMP441 (mic RX) e MAX98357A (amp TX) compartilham BCLK/WS no I2S_NUM_0.
 * Uma única chamada i2s_new_channel() cria ambos os canais TX+RX como master,
 * evitando o conflito de dois MASTER no mesmo port.
 *
 * Uso:
 *   audio_init();                          // boot — deve ser chamado uma vez
 *   audio_mic_read(buf, 256);              // leitura do microfone
 *   audio_play_pcm(buf, n);               // reprodução no amplificador
 *   audio_set_volume(80);                 // volume 0–100
 */

/* Inicializa I2S0 full-duplex: INMP441 (RX) + MAX98357A (TX), 16 kHz. */
void audio_init(void);

/*
 * Lê até `samples` amostras mono 16-bit do INMP441 (máx 256 por chamada).
 * Extrai o canal com maior magnitude do frame estéreo 32-bit.
 * Bloqueia até os dados ficarem disponíveis (timeout 1 s).
 * Retorna o número de amostras efetivamente lidas.
 */
size_t audio_mic_read(int16_t *buf, size_t samples);

/*
 * Envia amostras PCM mono 16-bit para o MAX98357A.
 * Volume aplicado internamente; buffer não é modificado.
 * Bloqueia até todos os dados serem enfileirados no DMA.
 */
void audio_play_pcm(const int16_t *buf, size_t samples);

/* Volume de software: 0 (mudo) – 100 (máximo). Padrão: 80. */
void audio_set_volume(uint8_t vol_pct);

/*
 * Gera onda senoidal em out_buf (não reproduz).
 * out_buf deve ter capacidade ≥ 16000 * duration_ms / 1000 amostras.
 * Retorna o número de amostras geradas.
 */
size_t audio_generate_beep(uint16_t freq_hz, uint32_t duration_ms, int16_t *out_buf);

#ifdef __cplusplus
}
#endif
