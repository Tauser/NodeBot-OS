#pragma once
/*
 * max98357a_driver.h — alias de compatibilidade.
 * O driver foi fundido em audio_driver.c (I2S0 full-duplex).
 * Inclua audio_driver.h e use audio_play_pcm() / audio_set_volume().
 */
#include "audio_driver.h"

#define max98357a_init()                 audio_init()
#define max98357a_play_pcm(buf, n)       audio_play_pcm((buf), (n))
#define max98357a_set_volume(v)          audio_set_volume((v))
#define generate_beep(f, d, buf)         audio_generate_beep((f), (d), (buf))
