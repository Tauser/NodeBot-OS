#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INTENT_UNKNOWN      = 0,
    INTENT_SLEEP,            /* "dorme"           */
    INTENT_WAKE,             /* "acorda"          */
    INTENT_SILENCE,          /* "silêncio"        */
    INTENT_PRIVACY_MODE,     /* "modo privado"    */
    INTENT_WHAT_TIME,        /* "que horas são"   */
    INTENT_HOW_ARE_YOU,      /* "como você está"  */
    INTENT_LOOK_AT_ME,       /* "me olha"         */
    INTENT_VOLUME_UP,        /* "volume alto"     */
    INTENT_VOLUME_DOWN,      /* "volume baixo"    */
    INTENT_YES,              /* "sim"             */
    INTENT_NO,               /* "não"             */
    INTENT_CANCEL,           /* "cancela"         */
    INTENT_COUNT             /* sentinela         */
} intent_t;

/*
 * Inicializa o IntentMapper.
 *
 * Aloca buffer de captura em PSRAM (96 KB / 3s).
 * Inicializa keyword_spotter (carrega templates do SD).
 * Subscreve EVT_WAKE_WORD no EventBus.
 * Cria intent_mapper_task (Core 1, P10).
 *
 * Se o keyword_spotter não tiver templates (SD ausente), o sistema
 * continua operando — EVT_INTENT_DETECTED é publicado com INTENT_UNKNOWN.
 */
esp_err_t intent_mapper_init(void);

/* Mapeia keyword_id → intent_t. */
intent_t intent_mapper_resolve(int keyword_id);

#ifdef __cplusplus
}
#endif
