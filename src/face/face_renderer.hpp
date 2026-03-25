#pragma once

#include <LovyanGFX.hpp>
#include "face_params.h"

/*
 * FaceRenderer — renderizador de olhos Cozmo/EMO estilo.
 *
 * Geometria base (landscape 320×240):
 *   EYE_W=82   EYE_H=78
 *   EYE_L_CX=96  EYE_R_CX=224  EYE_CY=120
 *
 * Cada olho é desenhado por preenchimento de colunas (drawFastVLine) com
 * clipping circular nos 4 cantos para arredondamento (arcTo equivalente).
 *
 * Camadas por olho (ordem de desenho):
 *   1. Glow — mesma forma 2 px maior, cor escura
 *   2. Shape principal — fillEyeColumns com rt_top / rb_bot
 *   3. Highlight — faixa superior 22% do olho, branqueada 22%
 */
class FaceRenderer {
public:
    void init(lgfx::LGFX_Sprite *spr);

    /* Limpa o sprite e renderiza dois olhos independentes conforme p.
     * dx/dy: offset de micro-movimento aplicado em render-time (não altera p). */
    void draw(const face_params_t &p, int dx = 0, int dy = 0);

private:
    lgfx::LGFX_Sprite *_spr = nullptr;

    /*
     * Renderiza um olho centrado em (cx, cy) com todos os parâmetros
     * individuais do olho extraídos de face_params_t.
     */
    void drawEye(int cx, int cy,
                 int8_t tl, int8_t tr, int8_t bl, int8_t br,
                 float open_f, int8_t oy,
                 uint8_t rt, uint8_t rb,
                 int8_t cv_top, int8_t cv_bot,
                 uint16_t color);

    /*
     * Preenchimento por colunas do quadrilátero definido por:
     *   topo: linha de (xl,ytl) a (xr,ytr)
     *   fundo: linha de (xl,ybl) a (xr,ybr)
     * Arredondamento: raio rt no topo, rb no fundo (clipping circular).
     * Curvatura: cv_top/cv_bot deslocam o centro da borda em px (parabólico).
     */
    static void fillEyeColumns(lgfx::LGFX_Sprite *spr,
                                int xl, int ytl, int xr, int ytr,
                                int ybl, int ybr,
                                int cv_top, int cv_bot,
                                int rt, int rb,
                                uint16_t color);
};
