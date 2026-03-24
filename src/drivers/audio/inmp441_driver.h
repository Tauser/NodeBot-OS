#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa I2S0 para INMP441: 16 kHz, mono, 32-bit container. */
void inmp441_init(void);

/*
 * Lê até `samples` amostras (máx 512 por chamada).
 * Extrai os 16 bits úteis do container de 32 bits (shift de 16).
 * Bloqueia até os dados ficarem disponíveis (timeout 1 s).
 * Retorna o número de amostras efetivamente lidas.
 */
size_t inmp441_read_samples(int16_t *buf, size_t samples);

#ifdef __cplusplus
}
#endif
