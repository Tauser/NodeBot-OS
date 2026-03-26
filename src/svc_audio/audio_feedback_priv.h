#pragma once
/*
 * audio_feedback_priv.h — estado compartilhado entre audio_feedback.c
 * e audio_playback_task.c. Não incluir em outros módulos.
 */

#include "audio_feedback.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    int16_t *samples;   /* NULL = som não disponível */
    size_t   n_samples;
} sound_buf_t;

extern sound_buf_t    g_sounds[SOUND_COUNT];
extern QueueHandle_t  g_audio_cmd_queue;
extern atomic_bool    g_playing;
