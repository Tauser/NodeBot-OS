#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Payload de EVT_WAKE_WORD */
typedef struct {
    float    confidence;    /* 0.0–1.0 (mapeado do resultado WakeNet) */
    uint32_t timestamp_ms;  /* esp_log_timestamp() no momento da detecção */
    uint8_t  word_index;    /* índice retornado pelo WakeNet (1-based)     */
} wake_word_event_t;

/**
 * Inicializa o engine WakeNet (ESP-SR).
 *
 * O modelo é alocado internamente pelo esp-sr usando PSRAM.
 * model_data / model_size são ignorados se o modelo estiver linkado
 * estaticamente via menuconfig (caso típico); passe NULL/0.
 *
 * Deve ser chamado APÓS audio_init().
 *
 * @return true  inicializado com sucesso
 *         false ESP-SR não disponível ou sem PSRAM suficiente
 */
bool wake_word_init(const void *model_data, size_t model_size);

/**
 * Processa um bloco de áudio PCM 16-bit mono @ 16 kHz.
 * Chamado internamente pela AudioCaptureTask a cada bloco.
 *
 * @param block    Buffer de amostras
 * @param samples  Número de amostras (deve ser ≥ chunk_size do WakeNet)
 * @return confidence em [0.0, 1.0] se detectado, 0.0 se não
 */
float wake_word_feed(const int16_t *block, size_t samples);

/**
 * Suprime publicação de EVT_WAKE_WORD por `ms` milissegundos.
 * Chamar quando playback de áudio iniciar para evitar auto-wake.
 */
void wake_word_suppress_ms(uint32_t ms);

/**
 * Retorna true se o wake word estiver inicializado e operante.
 */
bool wake_word_is_ready(void);

#ifdef __cplusplus
}
#endif
