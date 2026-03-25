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
 *   rt_top / rb_bot  — raio de arredondamento topo / fundo (padrão 20 px)
 *   color            — RGB565 (padrão FACE_EYE_COLOR = #00E5FF)
 *   transition_ms    — duração da interpolação para esta expressão
 *   priority         — prioridade; expressão de maior prioridade vence
 */
typedef struct {
    int8_t   tl_l, tr_l, bl_l, br_l;   /* cantos olho esquerdo (px)          */
    int8_t   tl_r, tr_r, bl_r, br_r;   /* cantos olho direito  (px)          */
    float    open_l, open_r;            /* abertura 0.0–1.0                   */
    int8_t   y_l, y_r;                  /* offset vertical por olho (px)      */
    uint8_t  rt_top, rb_bot;            /* raio arredondamento topo/fundo (px)*/
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
        (op),(op), (y),(y), \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, \
        FACE_EYE_COLOR, 0, 0 \
    })

/* ── 21 expressões pré-definidas ────────────────────────────────────────── */

/* NEUTRAL — retangular simétrico */
#define FACE_NEUTRAL      _FSYM(  0,   0,   0,   0, 1.00f,  0)

/* BLINK_HIGH — piscar fechando de cima */
#define FACE_BLINK_HIGH   _FSYM(  0,   0,  28,  28, 0.10f,-18)

/* HAPPY — arco superior fechado, inferior aberto */
#define FACE_HAPPY        _FSYM( 14,  14,  -8,  -8, 0.80f,  0)

/* GLEE — sorriso exagerado */
#define FACE_GLEE         _FSYM( 22,  22,  -6,  -6, 0.62f,  0)

/* BLINK_LOW — piscar fechando de baixo */
#define FACE_BLINK_LOW    _FSYM( 28,  28,   0,   0, 0.10f, 18)

/* SAD_DOWN — olhos asimétricos caindo (triste para baixo) */
#define FACE_SAD_DOWN \
    ((face_params_t){ \
         18, -12,  0,  0, \
        -12,  18,  0,  0, \
        0.74f, 0.74f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* SAD_UP — triste elevado */
#define FACE_SAD_UP \
    ((face_params_t){ \
         14, -10,  0,  0, \
        -10,  14,  0,  0, \
        0.72f, 0.72f, -8, -8, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* WORRIED — preocupado */
#define FACE_WORRIED \
    ((face_params_t){ \
        -16,  12,  0,  0, \
         12, -16,  0,  0, \
        0.84f, 0.84f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* FOCUSED — concentrado, olho semi-fechado uniforme */
#define FACE_FOCUSED      _FSYM( 10,  10,   6,   6, 0.76f,  0)

/* ANNOYED — irritado assimétrico */
#define FACE_ANNOYED \
    ((face_params_t){ \
         10,  22,  8,  8, \
         22,  10,  8,  8, \
        0.60f, 0.60f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* SURPRISED — todos os cantos abertos */
#define FACE_SURPRISED    _FSYM(-10, -10, -10, -10, 1.00f,  0)

/* SKEPTIC — olho esquerdo e direito com abertura diferente */
#define FACE_SKEPTIC \
    ((face_params_t){ \
          8,   8,  4,  4, \
         18,  18, 10, 10, \
        0.70f, 0.48f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* FRUS_BORED — frustrado/entediado */
#define FACE_FRUS_BORED   _FSYM( 16,  16,  12,  12, 0.50f,  0)

/* UNIMPRESSED — indiferente, quase fechado */
#define FACE_UNIMPRESSED  _FSYM( 22,  22,  16,  16, 0.34f,  0)

/* SLEEPY — sonolento, olho esquerdo quase fechado */
#define FACE_SLEEPY \
    ((face_params_t){ \
         22,  22, 14, 14, \
         10,  10,  6,  6, \
        0.30f, 0.74f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* SUSPICIOUS — desconfiado assimétrico */
#define FACE_SUSPICIOUS \
    ((face_params_t){ \
         10,  24,  6,  6, \
         24,  10,  6,  6, \
        0.56f, 0.56f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* SQUINT — franzindo forte */
#define FACE_SQUINT       _FSYM( 20,  20,  20,  20, 0.24f,  0)

/* ANGRY — bravo, V invertido */
#define FACE_ANGRY \
    ((face_params_t){ \
        -26,  26,  0,  0, \
         26, -26,  0,  0, \
        0.86f, 0.86f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* FURIOUS — furioso, V invertido exagerado */
#define FACE_FURIOUS \
    ((face_params_t){ \
        -32,  32,  0,  8, \
         32, -32,  0,  8, \
        0.78f, 0.78f,  0,  0, \
        FACE_RT_DEFAULT, FACE_RB_DEFAULT, FACE_EYE_COLOR, 0, 0 \
    })

/* SCARED — assustado, cantos levemente abertos e assimétricos */
#define FACE_SCARED       _FSYM(-12,  -8,  -6, -10, 0.94f,  0)

/* AWE — maravilhado, olhos totalmente abertos */
#define FACE_AWE          _FSYM(-12, -12, -12, -12, 1.00f,  0)

/* ── Aliases de compatibilidade ─────────────────────────────────────────── */
#define FACE_SAD    FACE_SAD_DOWN
#define FACE_TIRED  FACE_FRUS_BORED
