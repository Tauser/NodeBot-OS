#include "emotion_mapper.h"
#include "face_engine.h"
#include "face_params.h"

/* ── Tabela de mapeamento emoção → expressão ────────────────────────────── */
/*
 * Ordem: NEUTRAL, HAPPY, SAD, ANGRY, SURPRISED, SCARED
 * Cada entrada usa o macro correspondente de face_params.h.
 */
static const face_params_t s_map[EMOTION_COUNT] = {
    [EMOTION_NEUTRAL]   = FACE_NEUTRAL,
    [EMOTION_HAPPY]     = FACE_HAPPY,
    [EMOTION_SAD]       = FACE_SAD_DOWN,
    [EMOTION_ANGRY]     = FACE_ANGRY,
    [EMOTION_SURPRISED] = FACE_SURPRISED,
    [EMOTION_SCARED]    = FACE_SCARED,
};

static const char *const s_names[EMOTION_COUNT] = {
    [EMOTION_NEUTRAL]   = "NEUTRAL",
    [EMOTION_HAPPY]     = "HAPPY",
    [EMOTION_SAD]       = "SAD",
    [EMOTION_ANGRY]     = "ANGRY",
    [EMOTION_SURPRISED] = "SURPRISED",
    [EMOTION_SCARED]    = "SCARED",
};

/* ── API ─────────────────────────────────────────────────────────────────── */

void emotion_mapper_apply(emotion_t e, uint16_t transition_ms)
{
    if ((unsigned)e >= EMOTION_COUNT) return;

    face_params_t p = s_map[e];
    if (transition_ms > 0) {
        p.transition_ms = transition_ms;
    }
    face_engine_apply_params(&p);
}

const char *emotion_name(emotion_t e)
{
    if ((unsigned)e >= EMOTION_COUNT) return "UNKNOWN";
    return s_names[e];
}
