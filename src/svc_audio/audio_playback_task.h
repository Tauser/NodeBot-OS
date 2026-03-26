#pragma once

/*
 * audio_playback_task.h — interface interna entre audio_feedback e a task.
 * Use audio_feedback.h para a API pública.
 */

#include "audio_feedback.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    sound_id_t id;
} audio_cmd_t;

/**
 * Cria a AudioPlaybackTask e a AudioCommandQueue.
 * Chamado internamente por audio_feedback_init().
 */
void audio_playback_task_start(void);

#ifdef __cplusplus
}
#endif
