#pragma once
/*
 * inmp441_driver.h — alias de compatibilidade.
 * O driver foi fundido em audio_driver.c (I2S0 full-duplex).
 * Inclua audio_driver.h e use audio_mic_read() no lugar de inmp441_read_samples().
 */
#include "audio_driver.h"

#define inmp441_init()                   audio_init()
#define inmp441_read_samples(buf, n)     audio_mic_read((buf), (n))
