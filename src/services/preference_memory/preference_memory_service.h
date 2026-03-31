#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Estilos de resposta */
typedef enum {
    RESPONSE_STYLE_BRIEF   = 0,
    RESPONSE_STYLE_NORMAL  = 1,
    RESPONSE_STYLE_VERBOSE = 2,
} response_style_t;

/* Registro de sessão armazenado no SD (ring buffer de 7 entradas) */
typedef struct {
    uint32_t timestamp_s;   /* uptime em segundos no início da sessão */
    uint32_t duration_s;    /* duração da sessão em segundos           */
    uint32_t interactions;  /* número de interações na sessão          */
    float    mood_final;    /* valence final (MoodService)             */
} session_record_t;

/*
 * Inicializa o PreferenceMemoryService.
 * Abre namespace NVS "nb_prefs". Carrega user_name, volume, response_style.
 * Deve ser chamado após config_manager_init().
 */
esp_err_t preference_memory_init(void);

/*
 * Encerra a sessão atual e salva o registro no SD (ring buffer de 7).
 * Pode ser chamado no shutdown ou por timer periódico.
 */
void preference_memory_close_session(uint32_t duration_s,
                                     uint32_t interactions,
                                     float    mood_final);

/* Lê nome do usuário em buf (máx max_len bytes). Retorna "" se não configurado. */
void      preference_memory_get_name(char *buf, size_t max_len);
esp_err_t preference_memory_set_name(const char *name);

/* Volume: 0–100 */
uint8_t   preference_memory_get_volume(void);
esp_err_t preference_memory_set_volume(uint8_t vol);

/* Estilo de resposta */
response_style_t preference_memory_get_response_style(void);
esp_err_t        preference_memory_set_response_style(response_style_t s);

/*
 * Copia as N últimas sessões (até 7) em ordem cronológica para *out.
 * Retorna quantas foram copiadas.
 */
uint8_t preference_memory_get_sessions(session_record_t *out, uint8_t n);

#ifdef __cplusplus
}
#endif
