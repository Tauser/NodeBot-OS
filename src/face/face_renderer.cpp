#include "face_renderer.hpp"
#include "face_renderer.h"

#include "esp_log.h"
#include <cmath>

static const char *TAG = "FACE_RNDR";

/* ── Geometria base (landscape 320×240) ─────────────────────────────────── */
static constexpr int EYE_W    = 82;
static constexpr int EYE_H    = 78;
static constexpr int EYE_L_CX = 96;
static constexpr int EYE_R_CX = 224;
static constexpr int EYE_CY   = 120;
/* ── Helpers de cor ─────────────────────────────────────────────────────── */

/* Mistura 22% branco no RGB565 (highlight) */
static inline uint16_t hlColor(uint16_t c)
{
    uint8_t r = ((c >> 11) & 0x1Fu) + (uint8_t)((31u - ((c >> 11) & 0x1Fu)) * 22u / 100u);
    uint8_t g = ((c >>  5) & 0x3Fu) + (uint8_t)((63u - ((c >>  5) & 0x3Fu)) * 22u / 100u);
    uint8_t b = ((c      ) & 0x1Fu) + (uint8_t)((31u - ((c      ) & 0x1Fu)) * 22u / 100u);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static inline float clampf_local(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Escala uma cor RGB565 contra fundo preto para bordas parciais simples */
static inline uint16_t scaleColor565(uint16_t c, float alpha)
{
    if (alpha <= 0.0f) return 0u;
    if (alpha >= 1.0f) return c;

    const uint8_t r = (uint8_t)roundf(((float)((c >> 11) & 0x1Fu)) * alpha);
    const uint8_t g = (uint8_t)roundf(((float)((c >>  5) & 0x3Fu)) * alpha);
    const uint8_t b = (uint8_t)roundf(((float)((c      ) & 0x1Fu)) * alpha);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* ── fillEyeColumns ─────────────────────────────────────────────────────── */
/*
 * Preenche o quadrilátero:
 *   topo:  (xl,ytl) → (xr,ytr)   [borda diagonal]
 *   fundo: (xl,ybl) → (xr,ybr)   [borda diagonal]
 *   lados: xl e xr são verticais
 *
 * Para cada coluna x em [xl,xr]:
 *   - interpola yt/yb pela diagonal de topo e fundo
 *   - aplica clipping circular nos 4 cantos com raios rt (topo) e rb (fundo)
 *   - desenha drawFastVLine(x, yt, height, color)
 *
 * Equivalente a um path arcTo() nos 4 cantos.
 */
void FaceRenderer::fillEyeColumns(lgfx::LGFX_Sprite *spr,
                                   int xl, int ytl, int xr, int ytr,
                                   int ybl, int ybr,
                                   int cv_top, int cv_bot,
                                   int rt, int rb,
                                   uint16_t color)
{
    if (!spr || xl >= xr) return;
    const float ew_f = (float)(xr - xl);

    for (int x = xl; x <= xr; x++) {
        const float t = (x == xr) ? 1.0f : (float)(x - xl) / ew_f;
        /* Curvatura parabólica: pico no centro (t=0.5), zero nas bordas */
        const float bow = 4.0f * t * (1.0f - t);
        float yt_f = (float)ytl + t * (float)(ytr - ytl) - (float)cv_top * bow;
        float yb_f = (float)ybl + t * (float)(ybr - ybl) + (float)cv_bot * bow;

        const int dxl = x - xl;
        const int dxr = xr - x;

        /* Arredondamento canto superior-esquerdo
         * Círculo centrado em (xl+rt, ytl+rt), raio rt */
        if (rt > 0 && dxl < rt) {
            const float dx = (float)(dxl - rt);
            const float clip = (float)(ytl + rt) - sqrtf((float)(rt * rt) - dx * dx);
            if (yt_f < clip) yt_f = clip;
        }
        /* Arredondamento canto superior-direito
         * Círculo centrado em (xr-rt, ytr+rt), raio rt */
        if (rt > 0 && dxr < rt) {
            const float dx = (float)(dxr - rt);
            const float clip = (float)(ytr + rt) - sqrtf((float)(rt * rt) - dx * dx);
            if (yt_f < clip) yt_f = clip;
        }
        /* Arredondamento canto inferior-esquerdo
         * Círculo centrado em (xl+rb, ybl-rb), raio rb */
        if (rb > 0 && dxl < rb) {
            const float dx = (float)(dxl - rb);
            const float clip = (float)(ybl - rb) + sqrtf((float)(rb * rb) - dx * dx);
            if (yb_f > clip) yb_f = clip;
        }
        /* Arredondamento canto inferior-direito
         * Círculo centrado em (xr-rb, ybr-rb), raio rb */
        if (rb > 0 && dxr < rb) {
            const float dx = (float)(dxr - rb);
            const float clip = (float)(ybr - rb) + sqrtf((float)(rb * rb) - dx * dx);
            if (yb_f > clip) yb_f = clip;
        }

        if (yt_f > yb_f) {
            continue;
        }

        const int top_px = (int)floorf(yt_f);
        const int bot_px = (int)floorf(yb_f);

        if (top_px == bot_px) {
            const float cov = clampf_local(yb_f - yt_f, 0.0f, 1.0f);
            if (cov > 0.0f) {
                spr->drawPixel(x, top_px, scaleColor565(color, cov));
            }
            continue;
        }

        const int fill_y = top_px;
        const int fill_h = bot_px - fill_y + 1;
        if (fill_h > 0) {
            spr->drawFastVLine(x, fill_y, fill_h, color);
        }
    }
}

/* ── FaceRenderer::init ─────────────────────────────────────────────────── */
void FaceRenderer::init(lgfx::LGFX_Sprite *spr)
{
    _spr = spr;
}

/* ── FaceRenderer::drawEye ──────────────────────────────────────────────── */
void FaceRenderer::drawEye(int cx, int cy,
                            int8_t tl, int8_t tr,
                            int8_t bl, int8_t br,
                            float open_f, int8_t oy,
                            uint8_t rt, uint8_t rb,
                            int8_t cv_top, int8_t cv_bot,
                            uint16_t color)
{
    /* Clamp abertura */
    if (open_f < 0.05f) open_f = 0.05f;
    if (open_f > 1.0f)  open_f = 1.0f;

    const int hw = EYE_W / 2;
    int h = (int)((float)EYE_H * open_f);
    if (h < 6) h = 6;

    /* Para olhos estreitos, simplifica progressivamente a geometria para
     * evitar que curvatura e cantos comprimidos criem leitura de "blink". */
    if (h <= 18) {
        cv_top /= 2;
        cv_bot /= 2;
    }
    if (h <= 14) {
        rt = (uint8_t)((int)rt * 2 / 3);
        rb = (uint8_t)((int)rb * 2 / 3);
    }
    if (h <= 10) {
        cv_top = 0;
        cv_bot = 0;
    }
    if (h <= 8) {
        rt = 0;
        rb = 0;
    }

    /* Clamp raios para não exceder dimensões do olho */
    if ((int)rt > hw)  rt = (uint8_t)hw;
    if ((int)rb > hw)  rb = (uint8_t)hw;
    if ((int)rt > h/2) rt = (uint8_t)(h / 2);
    if ((int)rb > h/2) rb = (uint8_t)(h / 2);

    /*
     * Posições Y dos 4 cantos (spec):
     *   ytl = cy - h/2 + y + tl*open
     *   ytr = cy - h/2 + y + tr*open
     *   ybl = cy + h/2 + y + bl*open
     *   ybr = cy + h/2 + y + br*open
     */
    const int ytl = cy - h / 2 + (int)oy + (int)((float)tl * open_f);
    const int ytr = cy - h / 2 + (int)oy + (int)((float)tr * open_f);
    const int ybl = cy + h / 2 + (int)oy + (int)((float)bl * open_f);
    const int ybr = cy + h / 2 + (int)oy + (int)((float)br * open_f);

    const int xl = cx - hw;
    const int xr = cx + hw;

    /* Shape principal */
    fillEyeColumns(_spr, xl, ytl, xr, ytr, ybl, ybr,
                   (int)cv_top, (int)cv_bot,
                   (int)rt, (int)rb, color);

    /* Highlight desativado temporariamente:
     * a faixa superior estava criando uma divisao visual interna no olho. */
}

/* ── FaceRenderer::draw ─────────────────────────────────────────────────── */
void FaceRenderer::draw(const face_params_t &p, int dx, int dy)
{
    if (!_spr) {
        ESP_LOGE(TAG, "sprite não inicializado");
        return;
    }

    _spr->fillScreen(FACE_BG_COLOR);

    /* x_off: distância inter-ocular (128=padrão). Converte para deslocamento simétrico.
     * x_shift = (128 - di) / 2 → positivo = olhos mais próximos. */
    const int x_shift = (128 - (int)p.x_off) / 2;
    const int cx_l = EYE_L_CX + x_shift + dx;
    const int cx_r = EYE_R_CX - x_shift + dx;

    /* Olho esquerdo — dy de micro-movimento somado ao offset vertical */
    drawEye(cx_l, EYE_CY,
            p.tl_l, p.tr_l, p.bl_l, p.br_l,
            p.open_l, (int8_t)((int)p.y_l + dy),
            p.rt_top, p.rb_bot,
            p.cv_top, p.cv_bot, p.color);

    /* Olho direito */
    drawEye(cx_r, EYE_CY,
            p.tl_r, p.tr_r, p.bl_r, p.br_r,
            p.open_r, (int8_t)((int)p.y_r + dy),
            p.rt_top, p.rb_bot,
            p.cv_top, p.cv_bot, p.color);
}

/* ══════════════════════════════════════════════════════════════════════════
   Wrappers extern "C"
   ══════════════════════════════════════════════════════════════════════════ */

static FaceRenderer s_renderer;

extern "C" void face_renderer_set_sprite(void *sprite)
{
    s_renderer.init(static_cast<lgfx::LGFX_Sprite *>(sprite));
}

extern "C" void face_renderer_draw(const face_params_t *p)
{
    if (p) s_renderer.draw(*p);
}
