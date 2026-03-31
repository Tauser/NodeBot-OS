#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KWS_KEYWORDS  12   /* número de keywords suportadas */
#define KWS_NO_MATCH  (-1) /* nenhuma keyword reconhecida   */

typedef struct {
    int   keyword_id;   /* 0..KWS_KEYWORDS-1 ou KWS_NO_MATCH */
    float confidence;   /* 0.0..1.0                           */
} kws_result_t;

/*
 * Inicializa o keyword spotter.
 *
 * Carrega até 5 templates WAV por keyword do SD:
 *   {templates_dir}/{kw_name}_{0..4}.wav  — 16kHz, 16-bit mono.
 * Aloca buffers de trabalho em PSRAM.
 * Pré-computa janela Hanning, mel filterbank e matriz DCT.
 *
 * Retorna ESP_OK se ao menos 1 template foi carregado.
 * Se o SD não estiver disponível, retorna ESP_FAIL mas não causa pânico —
 * match() sempre retornará KWS_NO_MATCH.
 */
esp_err_t keyword_spotter_init(const char *templates_dir);

/*
 * Extrai MFCCs do áudio e compara com todos os templates via DTW.
 * Bloqueia a task chamadora (~50–300ms dependendo do tamanho do áudio).
 *
 * @param audio    PCM mono int16, 16 kHz
 * @param samples  Número de amostras
 */
kws_result_t keyword_spotter_match(const int16_t *audio, size_t samples);

/* Retorna o nome ASCII da keyword pelo ID (ex: "dorme"). */
const char *keyword_spotter_name(int keyword_id);

#ifdef __cplusplus
}
#endif
