#pragma once
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { PERSONA_CALM=0, PERSONA_PLAYFUL=1, PERSONA_MINIMAL=2 } persona_style_t;

/* Carrega /sdcard/persona.json; usa defaults se ausente. */
esp_err_t persona_service_init(void);
const char    *persona_get_name(void);
persona_style_t persona_get_style(void);

#ifdef __cplusplus
}
#endif
