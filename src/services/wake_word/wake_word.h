#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Inicializa o WakeWordService.
 *
 * Carrega modelos da partição SPIFFS "model".
 * Cria pipeline AFE (low-cost, mono mic) + WakeNet em PSRAM.
 * Cria WakeWordTask (Core 0, P15).
 *
 * Deve ser chamado após audio_capture_init() e event_bus_init().
 */
esp_err_t wake_word_init(void);

/**
 * Suprime detecção de wake word por `ms` milissegundos.
 *
 * Chamar ao iniciar playback de áudio para evitar auto-ativação.
 * Thread-safe (escrita atômica em uint32 alinhado em ARM).
 */
void wake_word_suppress_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
