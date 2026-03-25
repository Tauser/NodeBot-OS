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
        /* roundf em vez de truncar — reduz serrilado nas diagonais */
        int yt = (int)roundf((float)ytl + t * (float)(ytr - ytl) - (float)cv_top * bow);
        int yb = (int)roundf((float)ybl + t * (float)(ybr - ybl) + (float)cv_bot * bow);

        const int dxl = x - xl;
        const int dxr = xr - x;

        /* Arredondamento canto superior-esquerdo
         * Círculo centrado em (xl+rt, ytl+rt), raio rt */
        if (rt > 0 && dxl < rt) {
            const float dx = (float)(dxl - rt);
            const int clip = (int)roundf((float)(ytl + rt) - sqrtf((float)(rt * rt) - dx * dx));
            if (yt < clip) yt = clip;
        }
        /* Arredondamento canto superior-direito
         * Círculo centrado em (xr-rt, ytr+rt), raio rt */
        if (rt > 0 && dxr < rt) {
            const float dx = (float)(dxr - rt);
            const int clip = (int)roundf((float)(ytr + rt) - sqrtf((float)(rt * rt) - dx * dx));
            if (yt < clip) yt = clip;
        }
        /* Arredondamento canto inferior-esquerdo
         * Círculo centrado em (xl+rb, ybl-rb), raio rb */
        if (rb > 0 && dxl < rb) {
            const float dx = (float)(dxl - rb);
            const int clip = (int)roundf((float)(ybl - rb) + sqrtf((float)(rb * rb) - dx * dx));
            if (yb > clip) yb = clip;
        }
        /* Arredondamento canto inferior-direito
         * Círculo centrado em (xr-rb, ybr-rb), raio rb */
        if (rb > 0 && dxr < rb) {
            const float dx = (float)(dxr - rb);
            const int clip = (int)roundf((float)(ybr - rb) + sqrtf((float)(rb * rb) - dx * dx));
            if (yb > clip) yb = clip;
        }

        if (yt <= yb) {
            spr->drawFastVLine(x, yt, yb - yt + 1, color);
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
    if (h < 4) h = 4;

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

    /* 3. Highlight — faixa superior 22% do olho, cor branqueada 22% */
    const int hl_h = (int)((float)h * 0.22f);
    if (hl_h > 1) {
        int ybl_hl = ytl + hl_h;
        int ybr_hl = ytr + hl_h;
        if (ybl_hl > ybl) ybl_hl = ybl;
        if (ybr_hl > ybr) ybr_hl = ybr;
        fillEyeColumns(_spr, xl, ytl, xr, ytr,
                       ybl_hl, ybr_hl,
                       (int)cv_top, 0,
                       (int)rt, 0, hlColor(color));
    }
}

/* ── FaceRenderer::draw ─────────────────────────────────────────────────── */
void FaceRenderer::draw(const face_params_t &p)
{
    if (!_spr) {
        ESP_LOGE(TAG, "sprite não inicializado");
        return;
    }

    _spr->fillScreen(FACE_BG_COLOR);

    /* x_off >0 move olho esquerdo p/ direita e direito p/ esquerda */
    const int cx_l = EYE_L_CX + (int)p.x_off;
    const int cx_r = EYE_R_CX - (int)p.x_off;

    /* Olho esquerdo */
    drawEye(cx_l, EYE_CY,
            p.tl_l, p.tr_l, p.bl_l, p.br_l,
            p.open_l, p.y_l,
            p.rt_top, p.rb_bot,
            p.cv_top, p.cv_bot, p.color);

    /* Olho direito */
    drawEye(cx_r, EYE_CY,
            p.tl_r, p.tr_r, p.bl_r, p.br_r,
            p.open_r, p.y_r,
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
