#include "face_debug.h"
#include "display.h"

#include <LovyanGFX.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cinttypes>

static const char *TAG = "FACE_DBG";

/* ── constantes internas (aliases dos defines do .h) ─────────────────── */
static constexpr int CX       = FACE_DBG_EYE_CX;
static constexpr int CY       = FACE_DBG_EYE_CY;
static constexpr int EW       = FACE_DBG_EYE_W;
static constexpr int EH_MAX   = FACE_DBG_EYE_H_MAX;
static constexpr int PR_MIN   = FACE_DBG_PUPIL_R_MIN;
static constexpr int PR_MAX   = FACE_DBG_PUPIL_R_MAX;
static constexpr int BR_OFF   = FACE_DBG_BROW_OFFSET;
static constexpr int BR_W     = FACE_DBG_BROW_W;
static constexpr int BR_THICK = FACE_DBG_BROW_THICK;

/* ── estado global ────────────────────────────────────────────────────── */
static lgfx::LGFX_Sprite *s_spr = nullptr;

/* ── primitivas auxiliares ────────────────────────────────────────────── */

/* Estrela de 5 pontas desenhada com 10 triângulos a partir do centro */
static void draw_star(lgfx::LGFX_Sprite *s, int cx, int cy, int r)
{
    const float R  = (float)r;
    const float ri = R * 0.382f;             /* raio do círculo interno */
    const float base = -1.5707963f;          /* -π/2 → ponta para cima  */
    const float step = 6.2831853f / 5.0f;    /* 2π/5 */

    for (int i = 0; i < 5; i++) {
        float a0 = base + (float)i * step;
        float am = a0 + step * 0.5f;         /* ângulo do vértice interno */
        float a1 = a0 + step;

        int ox0 = cx + (int)(R  * cosf(a0));
        int oy0 = cy + (int)(R  * sinf(a0));
        int ix  = cx + (int)(ri * cosf(am));
        int iy  = cy + (int)(ri * sinf(am));
        int ox1 = cx + (int)(R  * cosf(a1));
        int oy1 = cy + (int)(R  * sinf(a1));

        s->fillTriangle(cx, cy, ox0, oy0, ix, iy,  TFT_BLACK);
        s->fillTriangle(cx, cy, ix, iy, ox1, oy1,  TFT_BLACK);
    }
}

/* Coração: dois semicírculos no topo + triângulo apontando para baixo */
static void draw_heart(lgfx::LGFX_Sprite *s, int cx, int cy, int r)
{
    int lobe_r = r / 2;              /* raio de cada lobo superior   */
    int top_y  = cy - lobe_r / 2;   /* centro y dos lobos           */

    s->fillCircle(cx - lobe_r, top_y, lobe_r, TFT_BLACK);
    s->fillCircle(cx + lobe_r, top_y, lobe_r, TFT_BLACK);

    /* triângulo: base na altura dos centros dos lobos, ponta abaixo */
    s->fillTriangle(
        cx - lobe_r * 2, top_y,
        cx + lobe_r * 2, top_y,
        cx,              cy + r,
        TFT_BLACK
    );
}

/* ── face_debug_set_sprite ─────────────────────────────────────────────── */
extern "C" void face_debug_set_sprite(void *sprite)
{
    s_spr = static_cast<lgfx::LGFX_Sprite *>(sprite);
}

/* ── face_debug_draw ───────────────────────────────────────────────────── */
extern "C" void face_debug_draw(float eyelid, float squint,
                                float brow_angle, int pupil,
                                uint16_t color)
{
    if (!s_spr) {
        ESP_LOGE(TAG, "sprite não configurado — chame face_debug_set_sprite()");
        return;
    }

    /* clamp de parâmetros */
    if (eyelid    < 0.0f) eyelid    = 0.0f;
    if (eyelid    > 1.0f) eyelid    = 1.0f;
    if (squint    < 0.0f) squint    = 0.0f;
    if (squint    > 1.0f) squint    = 1.0f;
    if (brow_angle < -1.0f) brow_angle = -1.0f;
    if (brow_angle >  1.0f) brow_angle =  1.0f;
    if (pupil < 0 || pupil > 4) pupil = 0;

    /* ── geometria do olho ────────────────────────────────────────── */
    int eye_h = (int)(eyelid * (float)EH_MAX);
    if (eye_h < 2) eye_h = 2;   /* mínimo visível */

    int top_y  = CY - eye_h / 2;
    int bot_y  = CY + eye_h / 2;
    int left_x = CX - EW / 2;
    int right_x = CX + EW / 2;

    /* ── 1. fundo ─────────────────────────────────────────────────── */
    s_spr->fillScreen(TFT_BLACK);

    /* ── 2. sobrancelha ───────────────────────────────────────────── */
    {
        /*
         * tilt > 0  (brow_angle > 0): interno (dir) sobe → right_y diminui
         * tilt < 0  (brow_angle < 0): externo (esq) sobe → left_y diminui
         */
        int tilt    = (int)(brow_angle * 12.0f);
        int brow_cy = top_y - BR_OFF;
        int lx      = CX - BR_W / 2;
        int rx      = CX + BR_W / 2;
        int ly      = brow_cy + tilt;
        int ry      = brow_cy - tilt;

        /* paralelogramo preenchido = 2 triângulos */
        s_spr->fillTriangle(lx, ly,
                            rx, ry,
                            rx, ry + BR_THICK,
                            color);
        s_spr->fillTriangle(lx, ly,
                            lx, ly + BR_THICK,
                            rx, ry + BR_THICK,
                            color);
    }

    /* ── 3. olho (base branca arredondada) ────────────────────────── */
    int corner_r = eye_h / 4;
    if (corner_r < 2) corner_r = 2;
    s_spr->fillRoundRect(left_x, top_y, EW, eye_h, corner_r, color);

    /* ── 4. squint — pálpebra inferior diagonal (lado interno sobe) ─ */
    if (squint > 0.01f) {
        int rise = (int)(squint * (float)eye_h * 0.55f);
        /*
         * A linha da pálpebra inferior vai de:
         *   (left_x,  bot_y)               — lado externo (esq) fica
         *   (right_x, bot_y - rise)         — lado interno (dir) sobe
         *
         * Triângulo negro que mascara a área abaixo da diagonal:
         *   vértices: canto inferior-esquerdo, canto inferior-direito,
         *             e o ponto onde a diagonal encontra o lado direito.
         */
        s_spr->fillTriangle(
            left_x,  bot_y,
            right_x, bot_y,
            right_x, bot_y - rise,
            TFT_BLACK
        );
    }

    /* ── 5. pupila ────────────────────────────────────────────────── */
    int pr  = PR_MIN + (int)((float)(PR_MAX - PR_MIN) * eyelid);
    int pcx = CX;
    int pcy = CY;

    switch (pupil) {
    case 0:  /* círculo normal */
        s_spr->fillCircle(pcx, pcy, pr, TFT_BLACK);
        break;

    case 1:  /* círculo pequeno */
        s_spr->fillCircle(pcx, pcy, PR_MIN, TFT_BLACK);
        break;

    case 2:  /* estrela de 5 pontas */
        draw_star(s_spr, pcx, pcy, pr);
        break;

    case 3:  /* coração */
        draw_heart(s_spr, pcx, pcy, pr);
        break;

    case 4:  /* oval vertical */
        s_spr->fillEllipse(pcx, pcy, pr * 2 / 3, pr, TFT_BLACK);
        break;
    }

    /* ── 6. empurra para o display ───────────────────────────────── */
    display_push_sprite(s_spr, 0, 0);

    ESP_LOGD(TAG, "draw  e=%.2f s=%.2f b=%.2f p=%d c=0x%04" PRIX16,
             eyelid, squint, brow_angle, pupil, color);
}

/* ── task serial ──────────────────────────────────────────────────────── */

/* Estado mantido entre comandos (parciais ou omitidos) */
struct DebugState {
    float    eyelid     = 0.8f;
    float    squint     = 0.0f;
    float    brow_angle = 0.0f;
    int      pupil      = 0;
    uint16_t color      = 0xFFFFu;   /* branco */
};

static void serial_task(void *arg)
{
    DebugState st{};

    /* Desenho inicial com valores default */
    face_debug_draw(st.eyelid, st.squint, st.brow_angle, st.pupil, st.color);

    ESP_LOGI(TAG, "=== face_debug pronto ===");
    ESP_LOGI(TAG, "Formato: e=<0-1> s=<0-1> b=<-1..1> p=<0-4> c=<RGB565>");
    ESP_LOGI(TAG, "Exemplo: e=0.80 s=0.40 b=-0.50 p=0 c=65535");

    char line[128];

    for (;;) {
        /* fgets bloqueia até receber newline ou EOF */
        if (!fgets(line, sizeof(line), stdin)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* remove \r\n */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        bool changed = false;

        /* Parseia cada token "chave=valor" independentemente */
        char *p = line;
        while (*p) {
            /* avança espaços */
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;

            char  key  = *p;
            char *eq   = strchr(p, '=');
            if (!eq) break;

            p = eq + 1;

            switch (key) {
            case 'e': { float v = strtof(p, &p); st.eyelid     = v; changed = true; break; }
            case 's': { float v = strtof(p, &p); st.squint      = v; changed = true; break; }
            case 'b': { float v = strtof(p, &p); st.brow_angle  = v; changed = true; break; }
            case 'p': { int   v = (int)strtol(p, &p, 10); st.pupil = v; changed = true; break; }
            case 'c': { unsigned long v = strtoul(p, &p, 10); st.color = (uint16_t)v; changed = true; break; }
            default:
                /* token desconhecido: avança até próximo espaço */
                while (*p && *p != ' ' && *p != '\t') p++;
                break;
            }
        }

        if (changed) {
            face_debug_draw(st.eyelid, st.squint, st.brow_angle, st.pupil, st.color);
            ESP_LOGI(TAG, "e=%.2f s=%.2f b=%+.2f p=%d c=0x%04" PRIX16,
                     st.eyelid, st.squint, st.brow_angle, st.pupil, st.color);
        }
    }
}

extern "C" void face_debug_start_task(void *sprite)
{
    if (sprite) {
        face_debug_set_sprite(sprite);
    } else {
        /* Cria sprite próprio em PSRAM — caller não precisa gerenciar nada */
        auto *spr = new lgfx::LGFX_Sprite();
        spr->setPsram(true);
        spr->setColorDepth(lgfx::rgb565_2Byte);
        void *buf = spr->createSprite(240, 320);
        if (!buf) {
            ESP_LOGE(TAG, "createSprite falhou — PSRAM insuficiente?");
            delete spr;
            return;
        }
        face_debug_set_sprite(spr);
        ESP_LOGI(TAG, "sprite interno criado em PSRAM  buf=%p", buf);
    }

    xTaskCreatePinnedToCore(
        serial_task,
        "FaceDbgTask",
        4096,
        nullptr,
        3,
        nullptr,
        0     /* Core 0 — face render usa Core 1 */
    );
    ESP_LOGI(TAG, "FaceDbgTask iniciada no Core 0");
}
