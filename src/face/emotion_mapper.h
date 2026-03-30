#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "face_params.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * emotion_t — 6 expressões base do robô.
 *
 * Cada emoção é mapeada para uma face_params_t pré-definida em face_params.h.
 * O mapper entrega a face base; gaze/runtime continua sendo responsabilidade
 * das camadas dinâmicas (E18/E20+).
 * Uso:
 *   emotion_mapper_apply(EMOTION_HAPPY, 400);   // transição de 400 ms
 */
typedef enum {
    EMOTION_NEUTRAL   = 0,
    EMOTION_HAPPY     = 1,
    EMOTION_SAD       = 2,
    EMOTION_FOCUSED   = 3,
    EMOTION_ANGRY     = 4,
    EMOTION_SURPRISED = 5,
    EMOTION_COUNT           /* sentinela — não usar como emoção */
} emotion_t;

/* Alias semântico útil para camadas de diálogo/atenção. */
#define EMOTION_THINKING EMOTION_FOCUSED

/*
 * Aplica a expressão correspondente a `e` via face_engine_apply_params().
 * `transition_ms` substitui o valor padrão da expressão; passe 0 para usar
 * o padrão da expressão.
 * Thread-safe (delega ao spinlock do face_engine).
 */
void emotion_mapper_apply(emotion_t e, uint16_t transition_ms);

/*
 * Copia a face base mapeada para `e` em *out.
 * Retorna false para emoção inválida.
 */
bool emotion_mapper_get(emotion_t e, face_params_t *out);

/*
 * Retorna o nome ASCII da emoção (útil para log).
 * Nunca retorna NULL.
 */
const char *emotion_name(emotion_t e);

#ifdef __cplusplus
}
#endif
