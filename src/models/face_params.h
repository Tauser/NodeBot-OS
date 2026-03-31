#pragma once

#include <stdint.h>

/*
 * face_params_t — parâmetros faciais simplificados.
 *
 * Mantém a geometria procedural dos olhos, gaze e squint.
 */

typedef enum {
    FACE_PRIORITY_COSMETIC = 0,
    FACE_PRIORITY_MOOD     = 1,
    FACE_PRIORITY_IDLE     = 2,
    FACE_PRIORITY_ROUTINE  = 3,
    FACE_PRIORITY_REACTION = 4,
    FACE_PRIORITY_DIALOGUE = 5,
    FACE_PRIORITY_SYSTEM   = 6,
    FACE_PRIORITY_SAFETY   = 7,
} face_priority_t;

typedef struct {
    int8_t   tl_l, tr_l, bl_l, br_l;
    int8_t   tl_r, tr_r, bl_r, br_r;
    float    open_l, open_r;
    int8_t   y_l, y_r;
    uint8_t  x_off;
    uint8_t  rt_top, rb_bot;
    int8_t   cv_top, cv_bot;
    uint16_t color;

    float           gaze_x, gaze_y;
    float           squint_l, squint_r;

    uint16_t        transition_ms;
    face_priority_t priority;
} face_params_t;

#define FACE_EYE_COLOR      0xFFFFu /*0x04B6u   teal RGB(0,148,180) — era 0x07FF (ciano puro=branco) */
#define FACE_BG_COLOR       0x0000u
#define FACE_BLUE_ACCENT    0x441A
#define FACE_YELLOW_ACCENT  0xFEA0
#define FACE_ORANGE_ACCENT  0xFC60
#define FACE_RED_ACCENT     0xF800
#define FACE_GREEN_ACCENT   0x05E0
#define FACE_WHITE_ACCENT   0xFFFFu

#define FACE_RT_DEFAULT     20u
#define FACE_RB_DEFAULT     20u

#define FACE_EXPR( \
    TL_L,TR_L,BL_L,BR_L, TL_R,TR_R,BL_R,BR_R, OPEN_L,OPEN_R, Y_L,Y_R, X_OFF, RT,RB, CV_T,CV_B, COLOR, \
    GAZE_X,GAZE_Y, SQUINT_L,SQUINT_R, MS, PRIO) \
    ((face_params_t){ \
        (TL_L),(TR_L),(BL_L),(BR_L), (TL_R),(TR_R),(BL_R),(BR_R), \
        (OPEN_L),(OPEN_R), (Y_L),(Y_R), (X_OFF), (RT),(RB), (CV_T),(CV_B), (uint16_t)(COLOR), \
        (GAZE_X),(GAZE_Y), (SQUINT_L),(SQUINT_R), \
        (MS), (PRIO) \
    })

#define FACE_NEUTRAL \
    FACE_EXPR( 0,0,0,0,0,0,0,0,0.95f,1.00f,0,0,96,20,20,0,0, FACE_EYE_COLOR, \
        -0.01f,0.00f, 0.00f,0.00f,500,FACE_PRIORITY_MOOD)
           

#define FACE_HAPPY \
    FACE_EXPR(10,10,-6,-6, 10,10,-6,-6, 0.50f,0.50f, -6,-6, 100, 8,8, 10,-12, FACE_YELLOW_ACCENT, \
              0.0f,0.0f, 0.40f,0.40f, 350, FACE_PRIORITY_MOOD)

#define FACE_GLEE \
    FACE_EXPR(16,16,-8,-18, 16,16,-18,-8, 0.42f,0.42f, -10,-10, 100, 8,8, 16,-18, FACE_ORANGE_ACCENT, \
              0.0f,0.0f, 0.55f,0.55f, 300, FACE_PRIORITY_REACTION)

#define FACE_SAD_DOWN \
    FACE_EXPR(10,-18,-8,-4, -18,10,-4,-8, 0.55f,0.55f, 18,18, 100, 12,12, -8,-6, FACE_BLUE_ACCENT, \
              0.0f,0.10f, 0.20f,0.20f, 500, FACE_PRIORITY_MOOD)

#define FACE_SAD_UP \
    FACE_EXPR(20,-8,0,-8, -8,20,-8,0, 0.68f,0.68f, -6,-6, 100, 8,8, 4,-10, FACE_BLUE_ACCENT, \
              0.0f,-0.20f, 0.12f,0.12f, 450, FACE_PRIORITY_MOOD)

#define FACE_WORRIED \
    FACE_EXPR(-8,-18,2,0, -18,-8,0,2, 0.82f,0.82f, 0,0, 98, 14,18, 0,0, FACE_BLUE_ACCENT, \
              0.0f,0.0f, 0.08f,0.08f, 450, FACE_PRIORITY_MOOD)

#define FACE_FOCUSED \
    FACE_EXPR(8,8,6,6, 8,8,6,6, 0.46f,0.46f, 0,0, 102, 14,6, 0,-8, FACE_EYE_COLOR, \
              0.0f,-0.10f, 0.15f,0.15f, 300, FACE_PRIORITY_DIALOGUE)

#define FACE_ANNOYED \
    FACE_EXPR(10,18,8,8, 18,10,8,8, 0.55f,0.55f, 4,4, 102, 8,12, 10,-4, FACE_ORANGE_ACCENT, \
              0.0f,0.0f, 0.45f,0.45f, 300, FACE_PRIORITY_REACTION)

#define FACE_SURPRISED \
    FACE_EXPR(-18,-18,-8,-8, -18,-18,-8,-8, 1.00f,1.00f, 0,0, 106, 24,24, 0,0, FACE_WHITE_ACCENT, \
              0.0f,0.0f, 0.0f,0.0f, 220, FACE_PRIORITY_REACTION)

#define FACE_SKEPTIC \
    FACE_EXPR(0,0,4,4, 16,0,8,8, 0.92f,0.64f, 0,6, 102, 8,18, 0,0, FACE_EYE_COLOR, \
              0.18f,0.0f, 0.0f,0.20f, 400, FACE_PRIORITY_IDLE)

#define FACE_FRUS_BORED \
    FACE_EXPR(18,12,10,10, 12,18,10,10, 0.42f,0.42f, 26,26, 100, 8,16, 0,0, FACE_EYE_COLOR, \
              0.0f,0.15f, 0.20f,0.20f, 500, FACE_PRIORITY_IDLE)

#define FACE_UNIMP \
    FACE_EXPR(18,18,14,14, 18,18,14,14, 0.34f,0.42f, 0,-4, 102, 8,18, 0,0, FACE_EYE_COLOR, \
              0.0f,0.0f, 0.10f,0.12f, 400, FACE_PRIORITY_IDLE)

#define FACE_SLEEPY \
    FACE_EXPR(22,14,12,6, 12,22,6,12, 0.28f,0.56f, 0,-10, 96, 14,14, 0,0, FACE_EYE_COLOR, \
              -0.12f,0.08f, 0.10f,0.25f, 700, FACE_PRIORITY_IDLE)

#define FACE_SUSPICIOUS \
    FACE_EXPR(8,8,6,6, 20,8,6,6, 0.58f,0.40f, 0,6, 96, 14,6, 0,0, FACE_GREEN_ACCENT, \
              0.12f,0.0f, 0.10f,0.20f, 420, FACE_PRIORITY_IDLE)

#define FACE_SQUINT \
    FACE_EXPR(16,16,10,16, 16,16,16,10, 0.42f,0.42f, 0,0, 96, 14,8, 0,0, FACE_EYE_COLOR, \
              0.0f,0.0f, 0.55f,0.55f, 350, FACE_PRIORITY_IDLE)

#define FACE_ANGRY \
    FACE_EXPR( 0,24,0,0,  24,0,0,0, 0.80f,0.80f,  0,0,  98,  9,4,  0,-5,  FACE_RED_ACCENT, \
    0.00f,0.00f,  0.80f,0.80f,  250,  FACE_PRIORITY_REACTION )

#define FACE_FURIOUS \
    FACE_EXPR(-8,28,-24,-20, 28,-2,-20,-24, 0.28f,0.28f, 0,0, 94, 6,6, 8,-14, FACE_RED_ACCENT, \
              0.0f,0.0f, 0.80f,0.80f, 220, FACE_PRIORITY_REACTION)

#define FACE_SCARED \
    FACE_EXPR(-2,-8,-6,-10, -8,-2,-10,-6, 0.90f,0.90f, 0,0, 94, 16,6, 0,-10, FACE_WHITE_ACCENT, \
              0.0f,0.0f, 0.0f,0.0f, 220, FACE_PRIORITY_REACTION)

#define FACE_AWE \
    FACE_EXPR(-22,-22,-14,-10, -22,-22,-10,-14, 1.00f,1.00f, 0,0, 92, 28,12, 0,-4, FACE_WHITE_ACCENT, \
              0.0f,-0.05f, 0.0f,0.0f, 300, FACE_PRIORITY_REACTION)

#define FACE_SAD   FACE_SAD_DOWN
#define FACE_TIRED FACE_FRUS_BORED
