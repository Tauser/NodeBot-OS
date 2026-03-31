#include "emotion_mapper.h"
#include "face_engine.h"
#include "event_bus.h"

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

/* ── Converte RGB565 → RGB88 com brightness cap (~12%) ──────────────────── */
static void rgb565_to_led(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t r5 = (color >> 11) & 0x1F;
    uint8_t g6 = (color >>  5) & 0x3F;
    uint8_t b5 =  color        & 0x1F;
    /* Escala para 0-30 (brightness ~12%) */
    *r = (r5 * 30u) / 31u;
    *g = (g6 * 30u) / 63u;
    *b = (b5 * 30u) / 31u;
}

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

    /* Sincroniza LEDs externos com a cor accent da expressão */
    led_cmd_t cmd;
    rgb565_to_led(p.color, &cmd.r, &cmd.g, &cmd.b);
    cmd.idx = LED_CMD_EMOTION;
    event_bus_publish(EVT_LED_CMD, &cmd, sizeof(cmd), EVENT_PRIO_COSMETIC);
}

const char *emotion_name(emotion_t e)
{
    if ((unsigned)e >= EMOTION_COUNT) return "UNKNOWN";
    return s_names[e];
}
