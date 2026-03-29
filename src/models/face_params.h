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
 *   x_off            — distância inter-ocular em px (128=padrão, menor=mais próximos)
 *   rt_top / rb_bot  — raio de arredondamento topo / fundo (padrão 20 px)
 *   cv_top / cv_bot  — curvatura parabólica topo / fundo (px no centro; 0=reto)
 *   color            — RGB565 (padrão FACE_EYE_COLOR = #0a97a7)
 *   transition_ms    — duração da interpolação para esta expressão
 *   priority         — prioridade; expressão de maior prioridade vence
 */
typedef struct {
    int8_t   tl_l, tr_l, bl_l, br_l;   /* cantos olho esquerdo (px)          */
    int8_t   tl_r, tr_r, bl_r, br_r;   /* cantos olho direito  (px)          */
    float    open_l, open_r;            /* abertura 0.0–1.0                   */
    int8_t   y_l, y_r;                  /* offset vertical por olho (px)      */
    uint8_t  x_off;                     /* dist. inter-ocular (px; 128=padrão, menor=mais próximos) */
    uint8_t  rt_top, rb_bot;            /* raio arredondamento topo/fundo (px)*/
    int8_t   cv_top, cv_bot;           /* curvatura parabólica topo/fundo(px)*/
    uint16_t color;                     /* RGB565                             */
    uint16_t transition_ms;             /* duração da transição (ms)          */
    uint8_t  priority;                  /* prioridade da expressão            */
} face_params_t;

/* ── Cores ──────────────────────────────────────────────────────────────── */
#define FACE_EYE_COLOR   0x07FF    /* #00E5FF cyan → RGB565                  */
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
        (op),(op), (y),(y), 128, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, \
        0, 0, \
        FACE_EYE_COLOR, 0, 0 \
    })

/* ── 21 expressões pré-definidas ────────────────────────────────────────── */
/* Linha 1:  tl_l  tr_l  bl_l  br_l  |  tl_r  tr_r  bl_r  br_r            */
/* Linha 2:  open_l  open_r  y_l  y_r  x_off                               */
/* Linha 3:  rt  rb  ct  cb  |  color  ms  pr                              */
/* x_off = (128 - di) / 2  (di=128 → 0, olhos na posição padrão)          */

/* Ordem: tl_l,tr_l,bl_l,br_l | tl_r,tr_r,bl_r,br_r | op_l,op_r | y_l,y_r | DIST | rt,rb | ct,cb | color | ms | pri */

#define FACE_NEUTRAL \
    ((face_params_t){ 0,0,0,0,  0,0,0,0,  1.00f,1.00f,  0,0,  108,  20,20,  0,0,  (uint16_t)FACE_EYE_COLOR, 500, 0 })

#define FACE_BLINK_HI \
    ((face_params_t){ 0,0,28,28,  0,0,28,28,  0.09f,0.09f,  -18,-18,  128,  20,20,  0,0,  (uint16_t)FACE_EYE_COLOR, 100, 1 })

#define FACE_HAPPY \
    ((face_params_t){ 12,12,-6,-6,  12,12,-6,-6,  0.38f,0.38f,  -8,-8,  116,  5,5,  12,-12,  (uint16_t)FACE_EYE_COLOR, 420, 0 })

#define FACE_GLEE \
    ((face_params_t){ 20,20,-4,-22,  20,20,-22,-4,  0.34f,0.34f,  -14,-14,  118,  4,4,  20,-18,  (uint16_t)FACE_EYE_COLOR, 420, 0 })

#define FACE_BLINK_LO \
    ((face_params_t){ 28,28,0,0,  28,28,0,0,  0.10f,0.10f,  18,18,  128,  20,20,  0,0,  (uint16_t)FACE_EYE_COLOR, 100, 1 })

#define FACE_SAD_DOWN \
    ((face_params_t){ 15,-18,-6,-4,  -18,15,-4,-6,  0.68f,0.68f,  34,34,  112,  10,10,  -7,-6,  (uint16_t)FACE_EYE_COLOR, 620, 0 })

#define FACE_SAD_UP \
    ((face_params_t){ 24,-8,0,-7,  -8,24,-7,0,  0.76f,0.76f,  -6,-6,  112,  5,5,  3,-9,  (uint16_t)FACE_EYE_COLOR, 620, 0 })

#define FACE_WORRIED \
    ((face_params_t){ -10,-24,2,0,  -24,-10,0,2,  0.88f,0.88f,  0,0,  108,  13,18,  0,0,  (uint16_t)FACE_EYE_COLOR, 520, 0 })

#define FACE_FOCUSED \
    ((face_params_t){ 8,8,5,5,  8,8,5,5,  0.58f,0.58f,  0,0,  114,  14,6,  0,-9,  (uint16_t)FACE_EYE_COLOR, 380, 0 })

#define FACE_ANNOYED \
    ((face_params_t){ 9,18,7,7,  18,9,7,7,  0.56f,0.64f,  4,0,  116,  5,13,  11,-3,  (uint16_t)FACE_EYE_COLOR, 430, 0 })

#define FACE_SURPRISED \
    ((face_params_t){ -20,-20,-9,-9,  -20,-20,-9,-9,  1.00f,1.00f,  0,0,  112,  24,24,  0,0,  (uint16_t)FACE_EYE_COLOR, 280, 0 })

#define FACE_SKEPTIC \
    ((face_params_t){ 0,0,3,3,  15,0,8,8,  0.96f,0.76f,  0,6,  116,  5,18,  0,0,  (uint16_t)FACE_EYE_COLOR, 520, 0 })

#define FACE_FRUS_BORED \
    ((face_params_t){ 18,10,8,8,  10,18,8,8,  0.60f,0.60f,  30,30,  114,  5,16,  0,0,  (uint16_t)FACE_EYE_COLOR, 620, 0 })

#define FACE_UNIMP \
    ((face_params_t){ 16,16,10,10,  16,16,10,10,  0.50f,0.54f,  0,-4,  114,  6,18,  0,0,  (uint16_t)FACE_EYE_COLOR, 540, 0 })

#define FACE_SLEEPY \
    ((face_params_t){ 22,14,10,4,  10,22,4,10,  0.48f,0.68f,  0,-8,  108,  14,14,  0,0,  (uint16_t)FACE_EYE_COLOR, 860, 0 })

#define FACE_SLEEP \
    ((face_params_t){ 8,0,0,3,  0,8,3,0,  0.15f,0.15f,  0,0,  108,  6,6,  -3,3,  (uint16_t)FACE_EYE_COLOR, 900, 0 })

#define FACE_SUSPICIOUS \
    ((face_params_t){ 10,10,6,6,  22,10,6,6,  0.60f,0.47f,  0,6,  100,  14,4,  0,0,  (uint16_t)FACE_EYE_COLOR, 450, 0 })

#define FACE_SQUINT \
    ((face_params_t){ 12,12,7,12,  12,12,12,7,  0.54f,0.54f,  0,0,  110,  14,6,  0,0,  (uint16_t)FACE_EYE_COLOR, 420, 0 })

#define FACE_ANGRY \
    ((face_params_t){ 10,24,0,0,  24,10,0,0,  0.90f,0.90f,  0,0,  106,  8,12,  0,0,  (uint16_t)FACE_EYE_COLOR, 430, 0 })

#define FACE_FURIOUS \
    ((face_params_t){ -8,26,-24,-22,  26,-1,-22,-24,  0.94f,0.94f,  0,0,  104,  8,6,  8,-12,  (uint16_t)FACE_EYE_COLOR, 320, 0 })

#define FACE_SCARED \
    ((face_params_t){ -2,-8,-5,-8,  -8,-2,-8,-5,  0.98f,0.98f,  0,0,  104,  18,5,  0,-10,  (uint16_t)FACE_EYE_COLOR, 230, 0 })

#define FACE_AWE \
    ((face_params_t){ -22,-22,-14,-10,  -22,-22,-10,-14,  1.00f,1.00f,  0,0,  102,  30,14,  0,-3,  (uint16_t)FACE_EYE_COLOR, 520, 0 })

/* ── Aliases de compatibilidade ─────────────────────────────────────────── */
#define FACE_SAD    FACE_SAD_DOWN
#define FACE_TIRED  FACE_FRUS_BORED
