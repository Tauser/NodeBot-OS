#pragma once

#include <stdint.h>

/*
 * face_params_t — parâmetros de expressão Cozmo/EMO com controle por olho independente.
 *
 * Cada olho é um quadrilátero deformado por 4 offsets de canto (px, 0=reto).
 * Positivo = move em direção ao centro (fecha), Negativo = abre.
 *
 * Convenção de nomes:
 *   tl_l / tr_l / bl_l / br_l — cantos do olho ESQUERDO (top-left, top-right, bot-left, bot-right)
 *   tl_r / tr_r / bl_r / br_r — cantos do olho DIREITO
 *
 * Campos:
 *   open_l / open_r  — abertura vertical (0.0 = fechado, 1.0 = completo)
 *   y_l / y_r        — offset vertical em px por olho (positivo = desce)
 *   x_off            — offset horizontal simétrico: >0 = olhos mais próximos
 *   rt_top / rb_bot  — raio de arredondamento topo / fundo (padrão 20 px)
 *   cv_top / cv_bot  — curvatura parabólica topo / fundo (px no centro; 0=reto)
 *   color            — RGB565 (padrão FACE_EYE_COLOR = #00E5FF)
 *   transition_ms    — duração da interpolação para esta expressão
 *   priority         — prioridade; expressão de maior prioridade vence
 */
typedef struct {
    int8_t   tl_l, tr_l, bl_l, br_l;   /* cantos olho esquerdo (px)          */
    int8_t   tl_r, tr_r, bl_r, br_r;   /* cantos olho direito  (px)          */
    float    open_l, open_r;            /* abertura 0.0–1.0                   */
    int8_t   y_l, y_r;                  /* offset vertical por olho (px)      */
    int8_t   x_off;                     /* offset horiz. simétrico (px)       */
    uint8_t  rt_top, rb_bot;            /* raio arredondamento topo/fundo (px)*/
    int8_t   cv_top, cv_bot;           /* curvatura parabólica topo/fundo(px)*/
    uint16_t color;                     /* RGB565                             */
    uint16_t transition_ms;             /* duração da transição (ms)          */
    uint8_t  priority;                  /* prioridade da expressão            */
} face_params_t;

/* ── Cores ──────────────────────────────────────────────────────────────── */
#define FACE_EYE_COLOR   0x073Fu   /* #00E5FF cyan → RGB565                  */
#define FACE_BG_COLOR    0x0000u   /* preto                                  */

/* ── Raios padrão ───────────────────────────────────────────────────────── */
#define FACE_RT_DEFAULT  20u
#define FACE_RB_DEFAULT  20u

/* ── Macro auxiliar (olhos simétricos) ─────────────────────────────────── */
/*   _TL _TR _BL _BR  open  y                                               */
#define _FSYM(tl,tr,bl,br,op,y) \
    ((face_params_t){ \
        (tl),(tr),(bl),(br), \
        (tl),(tr),(bl),(br), \
        (op),(op), (y),(y), 0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, \
        0, 0, \
        FACE_EYE_COLOR, 0, 0 \
    })

/* ── 21 expressões pré-definidas ────────────────────────────────────────── */
/* Linha 1:  tl_l  tr_l  bl_l  br_l  |  tl_r  tr_r  bl_r  br_r            */
/* Linha 2:  open_l  open_r  y_l  y_r  x_off                               */
/* Linha 3:  rt  rb  ct  cb  |  color  ms  pr                              */
/* x_off = (128 - di) / 2  (di=128 → 0, olhos na posição padrão)          */

#define FACE_NEUTRAL \
    ((face_params_t){   0,   0,   0,   0,    0,   0,   0,   0, \
                     1.00f,1.00f,   0,   0,  17, \
                        20,  20,   3,   3, FACE_EYE_COLOR, 500, 0 })

#define FACE_BLINK_HIGH \
    ((face_params_t){   0,   0,  28,  28,    0,   0,  28,  28, \
                     0.10f,0.10f, -18, -18,   0, \
                        20,  20,   0,   0, FACE_EYE_COLOR, 100, 1 })

#define FACE_HAPPY \
    ((face_params_t){  14,  14,  -8,  -8,   14,  14,  -8,  -8, \
                     0.32f,0.32f, -10, -10,   0, \
                         4,   4,  15, -15, FACE_EYE_COLOR, 400, 0 })

#define FACE_GLEE \
    ((face_params_t){  22,  22,  -6, -30,   22,  22, -30,  -6, \
                     0.28f,0.28f, -18, -18,   0, \
                         4,   4,  24, -24, FACE_EYE_COLOR, 400, 0 })

#define FACE_BLINK_LOW \
    ((face_params_t){  28,  28,   0,   0,   28,  28,   0,   0, \
                     0.10f,0.10f,  18,  18,   0, \
                        20,  20,   0,   0, FACE_EYE_COLOR, 100, 1 })

#define FACE_SAD_DOWN \
    ((face_params_t){  18, -22,  -8,  -5,  -22,  18,  -5,  -8, \
                     0.62f,0.62f,  40,  40,   0, \
                        10,  10,  -8,  -7, FACE_EYE_COLOR, 600, 0 })

#define FACE_SAD_UP \
    ((face_params_t){  30, -10,   0,  -9,  -10,  30,  -9,   0, \
                     0.72f,0.72f,  -8,  -8,   0, \
                         4,   4,   4, -11, FACE_EYE_COLOR, 600, 0 })

#define FACE_WORRIED \
    ((face_params_t){ -12, -30,   2,   0,  -30, -12,   0,   2, \
                     0.84f,0.84f,   0,   0,  17, \
                        13,  19,   6,   0, FACE_EYE_COLOR, 500, 0 })

#define FACE_FOCUSED \
    ((face_params_t){  10,  10,   6,   6,   10,  10,   6,   6, \
                     0.44f,0.44f,   0,   0,   0, \
                        14,   4,   0, -11, FACE_EYE_COLOR, 350, 0 })

#define FACE_ANNOYED \
    ((face_params_t){  10,  22,   8,   8,   22,  10,   8,   8, \
                     0.50f,0.60f,   6,   0,   0, \
                        14,  14,   4,  -4, FACE_EYE_COLOR, 400, 0 })

#define FACE_SURPRISED \
    ((face_params_t){ -22, -22, -10, -10,  -22, -22, -10, -10, \
                     1.00f,1.00f,   0,   0,   0, \
                        25,  25,   3,   4, FACE_EYE_COLOR, 250, 0 })

#define FACE_SKEPTIC \
    ((face_params_t){   0,   0,   4,   4,   19,   0,  10,  10, \
                     1.00f,0.69f,   0,   8,   0, \
                         4,  20,   3,   3, FACE_EYE_COLOR, 500, 0 })

#define FACE_FRUS_BORED \
    ((face_params_t){  24,  13,  12,  12,   13,  24,  12,  12, \
                     0.46f,0.46f,  40,  40,   0, \
                         4,  17,   0,   0, FACE_EYE_COLOR, 600, 0 })

#define FACE_UNIMPRESSED \
    ((face_params_t){  22,  22,  16,  16,   22,  22,  16,  16, \
                     0.34f,0.43f,   0,  -6,   0, \
                         4,  20,   0,   0, FACE_EYE_COLOR, 500, 0 })

#define FACE_SLEEPY \
    ((face_params_t){  30,  19,  15,   5,   13,  30,   5,  15, \
                     0.30f,0.59f,   0, -13,  17, \
                        14,  14,   0,   0, FACE_EYE_COLOR, 800, 0 })

#define FACE_SUSPICIOUS \
    ((face_params_t){  10,  10,   6,   6,   24,  10,   6,   6, \
                     0.56f,0.41f,   0,   7,  18, \
                        14,   4,   0,   0, FACE_EYE_COLOR, 450, 0 })

#define FACE_SQUINT \
    ((face_params_t){  20,  20,  10,  20,   20,  20,  20,  10, \
                     0.40f,0.40f,   0,   0,  18, \
                        14,   4,   0,   0, FACE_EYE_COLOR, 400, 0 })

#define FACE_ANGRY \
    ((face_params_t){  12,  30,   0,   0,   30,  12,   0,   0, \
                     0.86f,0.86f,   0,   0,  18, \
                         8,  11,   0,   0, FACE_EYE_COLOR, 400, 0 })

#define FACE_FURIOUS \
    ((face_params_t){ -10,  30, -29, -27,   30,  -1, -27, -29, \
                     0.92f,0.92f,   0,   0,  18, \
                         8,   5,  10, -14, FACE_EYE_COLOR, 300, 0 })

#define FACE_SCARED \
    ((face_params_t){  -2,  -8,  -6, -10,   -8,  -2, -10,  -6, \
                     0.94f,0.94f,   0,   0,  18, \
                        25,   4,   2, -12, FACE_EYE_COLOR, 200, 0 })

#define FACE_AWE \
    ((face_params_t){ -27, -27, -18, -13,  -27, -27, -13, -18, \
                     1.00f,1.00f,   0,   0,  19, \
                        14,  14,   9,  -7, FACE_EYE_COLOR, 500, 0 })

/* ── Aliases de compatibilidade ─────────────────────────────────────────── */
#define FACE_SAD    FACE_SAD_DOWN
#define FACE_TIRED  FACE_FRUS_BORED
