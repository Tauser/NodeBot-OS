#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * emotion_t — 6 emoções primárias do robô.
 *
 * Cada emoção é mapeada para uma face_params_t pré-definida em face_params.h.
 * Uso:
 *   emotion_mapper_apply(EMOTION_HAPPY, 400);   // transição de 400 ms
 */
typedef enum {
    EMOTION_NEUTRAL   = 0,
    EMOTION_HAPPY     = 1,
    EMOTION_SAD       = 2,
    EMOTION_ANGRY     = 3,
    EMOTION_SURPRISED = 4,
    EMOTION_SCARED    = 5,
    EMOTION_COUNT           /* sentinela — não usar como emoção */
} emotion_t;

/*
 * Aplica a expressão correspondente a `e` via face_engine_apply_params().
 * `transition_ms` substitui o valor padrão da expressão; passe 0 para usar
 * o padrão da expressão.
 * Thread-safe (delega ao spinlock do face_engine).
 */
void emotion_mapper_apply(emotion_t e, uint16_t transition_ms);

/*
 * Retorna o nome ASCII da emoção (útil para log).
 * Nunca retorna NULL.
 */
const char *emotion_name(emotion_t e);

#ifdef __cplusplus
}
#endif
