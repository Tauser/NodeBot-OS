#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Callback invocado com a transcrição (NULL se timeout/falha). */
typedef void (*cloud_stt_cb_t)(const char *transcript);

/*
 * Inicializa o CloudBridge.
 * Lê OpenAI API key do NVS (namespace "wb_cloud", chave "openai_key").
 * Cria CloudTask (Core 0, P8).
 * Aloca buffer de áudio em PSRAM (96 KB — 3s @ 16 kHz).
 */
esp_err_t cloud_bridge_init(void);

/*
 * Salva a OpenAI API key no NVS.
 * Chamar uma vez durante provisionamento.
 * key: string "sk-..." com pelo menos 10 caracteres.
 */
esp_err_t cloud_bridge_set_api_key(const char *key);

/* true se WiFi conectado e API key configurada. */
bool cloud_bridge_is_available(void);

/*
 * Enfileira request STT — não bloqueia (retorna em < 1 ms).
 * audio: PCM 16-bit 16 kHz mono. samples: número de amostras.
 *
 * cb é invocado da CloudTask (Core 0) com a transcrição Whisper,
 * ou com NULL em caso de falha ou timeout. cb deve ser curto e não bloquear.
 *
 * Política de timeout:
 *   T_local  300 ms — resposta local já disponível antes deste prazo
 *   T_soft   600 ms — fallback local como primário; resultado cloud ainda útil
 *   T_hard  1200 ms — abort HTTP, cb(NULL), loga "cloud_timeout_dropped"
 *
 * Se WiFi desconectado, key ausente, ou request anterior ainda em curso:
 * cb(NULL) é chamado imediatamente.
 */
void cloud_bridge_request_stt(const int16_t *audio, size_t samples,
                               cloud_stt_cb_t cb);

#ifdef __cplusplus
}
#endif
