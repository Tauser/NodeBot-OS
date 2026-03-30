#include "emotion_mapper.h"
#include "face_engine.h"

/* ── Tabela de mapeamento emoção → expressão ────────────────────────────── */
/*
 * Ordem: NEUTRAL, HAPPY, SAD, FOCUSED, ANGRY, SURPRISED
 * Cada entrada usa o macro correspondente de face_params.h.
 */
static const face_params_t s_map[EMOTION_COUNT] = {
    [EMOTION_NEUTRAL]   = FACE_NEUTRAL,
    [EMOTION_HAPPY]     = FACE_HAPPY,
    [EMOTION_SAD]       = FACE_SAD_DOWN,
    [EMOTION_FOCUSED]   = FACE_FOCUSED,
    [EMOTION_ANGRY]     = FACE_ANGRY,
    [EMOTION_SURPRISED] = FACE_SURPRISED,
};

static const char *const s_names[EMOTION_COUNT] = {
    [EMOTION_NEUTRAL]   = "NEUTRAL",
    [EMOTION_HAPPY]     = "HAPPY",
    [EMOTION_SAD]       = "SAD",
    [EMOTION_FOCUSED]   = "FOCUSED",
    [EMOTION_ANGRY]     = "ANGRY",
    [EMOTION_SURPRISED] = "SURPRISED",
};

/* ── API ─────────────────────────────────────────────────────────────────── */

bool emotion_mapper_get(emotion_t e, face_params_t *out)
{
    if (!out || (unsigned)e >= EMOTION_COUNT) return false;

    *out = s_map[e];

    /* E19 entrega a face base; gaze/social tracking ficam em runtime. */
    out->gaze_x = 0.0f;
    out->gaze_y = 0.0f;
    out->priority = FACE_PRIORITY_MOOD;
    return true;
}

void emotion_mapper_apply(emotion_t e, uint16_t transition_ms)
{
    face_params_t p;
    if (!emotion_mapper_get(e, &p)) return;
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
