#include "face_renderer.hpp"
#include "face_renderer.h"

#include "esp_log.h"
#include <cmath>

static const char *TAG = "FACE_RNDR";

static constexpr int EYE_W    = 82;
static constexpr int EYE_H    = 78;
static constexpr int EYE_L_CX = 96;
static constexpr int EYE_R_CX = 224;
static constexpr int EYE_CY   = 120;

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

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
        const float bow = 4.0f * t * (1.0f - t);
        int yt = (int)roundf((float)ytl + t * (float)(ytr - ytl) - (float)cv_top * bow);
        int yb = (int)roundf((float)ybl + t * (float)(ybr - ybl) + (float)cv_bot * bow);

        const int dxl = x - xl;
        const int dxr = xr - x;

        if (rt > 0 && dxl < rt) {
            const float dx = (float)(dxl - rt);
            const int clip = (int)roundf((float)(ytl + rt) - sqrtf((float)(rt * rt) - dx * dx));
            if (yt < clip) yt = clip;
        }
        if (rt > 0 && dxr < rt) {
            const float dx = (float)(dxr - rt);
            const int clip = (int)roundf((float)(ytr + rt) - sqrtf((float)(rt * rt) - dx * dx));
            if (yt < clip) yt = clip;
        }
        if (rb > 0 && dxl < rb) {
            const float dx = (float)(dxl - rb);
            const int clip = (int)roundf((float)(ybl - rb) + sqrtf((float)(rb * rb) - dx * dx));
            if (yb > clip) yb = clip;
        }
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

void FaceRenderer::init(lgfx::LGFX_Sprite *spr)
{
    _spr = spr;
}

void FaceRenderer::drawEye(int cx, int cy,
                           int8_t tl, int8_t tr,
                           int8_t bl, int8_t br,
                           float open_f, int8_t oy,
                           uint8_t rt, uint8_t rb,
                           int8_t cv_top, int8_t cv_bot,
                           float squint,
                           uint16_t color)
{
    if (!_spr) return;

    open_f = clampf(open_f, 0.03f, 1.0f);
    squint = clampf(squint, 0.0f, 1.0f);

    const int hw = EYE_W / 2;
    const int h = clampi((int)roundf((float)EYE_H * open_f), 4, EYE_H);

    if ((int)rt > hw)  rt = (uint8_t)hw;
    if ((int)rb > hw)  rb = (uint8_t)hw;
    if ((int)rt > h / 2) rt = (uint8_t)(h / 2);
    if ((int)rb > h / 2) rb = (uint8_t)(h / 2);

    const int ytl = cy - h / 2 + (int)oy + (int)roundf((float)tl * open_f);
    const int ytr = cy - h / 2 + (int)oy + (int)roundf((float)tr * open_f);
    const int ybl = cy + h / 2 + (int)oy + (int)roundf((float)bl * open_f);
    const int ybr = cy + h / 2 + (int)oy + (int)roundf((float)br * open_f);
    const int xl = cx - hw;
    const int xr = cx + hw;

    fillEyeColumns(_spr, xl, ytl, xr, ytr, ybl, ybr, (int)cv_top, (int)cv_bot, (int)rt, (int)rb, color);

    const int rise = (int)roundf((float)h * 0.55f * squint);
    if (rise > 0) {
        _spr->fillTriangle(xl, ybl, xr, ybr, xr, ybr - rise, FACE_BG_COLOR);
        _spr->fillRect(xl, ybl + 1, EYE_W + 1, rise + 4, FACE_BG_COLOR);
    }

}

void FaceRenderer::draw(const face_params_t &p, int dx, int dy)
{
    if (!_spr) {
        ESP_LOGE(TAG, "sprite nao inicializado");
        return;
    }

    _spr->fillScreen(FACE_BG_COLOR);

    const int x_shift = (128 - (int)p.x_off) / 2;
    const int cx_l = EYE_L_CX + x_shift + dx;
    const int cx_r = EYE_R_CX - x_shift + dx;

    drawEye(cx_l, EYE_CY,
            p.tl_l, p.tr_l, p.bl_l, p.br_l,
            p.open_l, (int8_t)((int)p.y_l + dy),
            p.rt_top, p.rb_bot,
            p.cv_top, p.cv_bot,
            p.squint_l,
            p.color);

    drawEye(cx_r, EYE_CY,
            p.tl_r, p.tr_r, p.bl_r, p.br_r,
            p.open_r, (int8_t)((int)p.y_r + dy),
            p.rt_top, p.rb_bot,
            p.cv_top, p.cv_bot,
            p.squint_r,
            p.color);
}

static FaceRenderer s_renderer;

extern "C" void face_renderer_set_sprite(void *sprite)
{
    s_renderer.init(static_cast<lgfx::LGFX_Sprite *>(sprite));
}

extern "C" void face_renderer_draw(const face_params_t *p)
{
    if (p) s_renderer.draw(*p);
}
