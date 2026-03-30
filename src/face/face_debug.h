#pragma once

/*
 * face_debug — ferramenta de calibração de olho via serial.
 *
 * Uso standalone: NÃO deve rodar simultaneamente com FaceEngine, pois ambos
 * escrevem no display sem sincronização. Para calibrar, pause face_engine
 * (não inicie face_engine_start_task) e chame face_debug_start_task().
 *
 * Protocolo serial:
 *   e=<0.0-1.0>  s=<0.0-1.0>  c=<RGB565>
 *   Exemplo: e=0.80 s=0.40 c=65535
 *   Campos omitidos mantêm o valor anterior.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FACE_DBG_EYE_CX       120
#define FACE_DBG_EYE_CY       160
#define FACE_DBG_EYE_W         56
#define FACE_DBG_EYE_H_MAX     56

void face_debug_set_sprite(void *sprite);
void face_debug_draw(float eyelid, float squint, uint16_t color);
void face_debug_start_task(void *sprite);

#ifdef __cplusplus
}
#endif
