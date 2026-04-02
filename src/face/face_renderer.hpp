#pragma once

#include <LovyanGFX.hpp>
#include "face_params.h"

/*
 * FaceRenderer — renderizador procedural simplificado.
 */
class FaceRenderer {
public:
    void init(lgfx::LGFX_Sprite *spr);
    void draw(const face_params_t &p, int dx = 0, int dy = 0);

private:
    lgfx::LGFX_Sprite *_spr = nullptr;

    void drawEye(int cx, int cy,
                 int8_t tl, int8_t tr, int8_t bl, int8_t br,
                 float open_f, int8_t oy,
                 uint8_t rt, uint8_t rb,
                 int8_t cv_top, int8_t cv_bot,
                 float squint,
                 float width_scale,
                 uint16_t color,
                 bool mirror = false);

    static void fillEyeColumns(lgfx::LGFX_Sprite *spr,
                               int xl, int ytl, int xr, int ytr,
                               int ybl, int ybr,
                               int cv_top, int cv_bot,
                               int rt, int rb,
                               uint16_t color);
};
