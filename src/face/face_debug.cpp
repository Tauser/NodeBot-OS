#include "face_debug.h"
#include "display.h"

#include <LovyanGFX.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <cstdio>
#include <cstring>
#include <cinttypes>

static const char *TAG = "FACE_DBG";

static constexpr int CX     = FACE_DBG_EYE_CX;
static constexpr int CY     = FACE_DBG_EYE_CY;
static constexpr int EW     = FACE_DBG_EYE_W;
static constexpr int EH_MAX = FACE_DBG_EYE_H_MAX;

static lgfx::LGFX_Sprite *s_spr = nullptr;

extern "C" void face_debug_set_sprite(void *sprite)
{
    s_spr = static_cast<lgfx::LGFX_Sprite *>(sprite);
}

extern "C" void face_debug_draw(float eyelid, float squint, uint16_t color)
{
    if (!s_spr) {
        ESP_LOGE(TAG, "sprite nao configurado");
        return;
    }

    if (eyelid < 0.0f) eyelid = 0.0f;
    if (eyelid > 1.0f) eyelid = 1.0f;
    if (squint < 0.0f) squint = 0.0f;
    if (squint > 1.0f) squint = 1.0f;

    int eye_h = (int)(eyelid * (float)EH_MAX);
    if (eye_h < 2) eye_h = 2;

    const int top_y   = CY - eye_h / 2;
    const int bot_y   = CY + eye_h / 2;
    const int left_x  = CX - EW / 2;
    const int right_x = CX + EW / 2;

    s_spr->fillScreen(TFT_BLACK);

    int corner_r = eye_h / 4;
    if (corner_r < 2) corner_r = 2;
    s_spr->fillRoundRect(left_x, top_y, EW, eye_h, corner_r, color);

    if (squint > 0.01f) {
        const int rise = (int)(squint * (float)eye_h * 0.55f);
        s_spr->fillTriangle(left_x, bot_y, right_x, bot_y, right_x, bot_y - rise, TFT_BLACK);
    }

    display_push_sprite(s_spr, 0, 0);
    ESP_LOGD(TAG, "draw e=%.2f s=%.2f c=0x%04" PRIX16, eyelid, squint, color);
}

struct DebugState {
    float    eyelid = 0.8f;
    float    squint = 0.0f;
    uint16_t color  = 0xFFFFu;
};

static void serial_task(void *arg)
{
    (void)arg;
    DebugState st{};
    face_debug_draw(st.eyelid, st.squint, st.color);

    ESP_LOGI(TAG, "=== face_debug pronto ===");
    ESP_LOGI(TAG, "Formato: e=<0-1> s=<0-1> c=<RGB565>");
    ESP_LOGI(TAG, "Exemplo: e=0.80 s=0.40 c=65535");

    char line[128];
    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        bool changed = false;
        char *p = line;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;

            char key = *p;
            char *eq = strchr(p, '=');
            if (!eq) break;
            p = eq + 1;

            switch (key) {
            case 'e': { float v = strtof(p, &p); st.eyelid = v; changed = true; break; }
            case 's': { float v = strtof(p, &p); st.squint = v; changed = true; break; }
            case 'c': { unsigned long v = strtoul(p, &p, 10); st.color = (uint16_t)v; changed = true; break; }
            default:
                while (*p && *p != ' ' && *p != '\t') p++;
                break;
            }
        }

        if (changed) {
            face_debug_draw(st.eyelid, st.squint, st.color);
            ESP_LOGI(TAG, "e=%.2f s=%.2f c=0x%04" PRIX16, st.eyelid, st.squint, st.color);
        }
    }
}

extern "C" void face_debug_start_task(void *sprite)
{
    if (sprite) {
        face_debug_set_sprite(sprite);
    } else {
        auto *spr = new lgfx::LGFX_Sprite();
        spr->setPsram(true);
        spr->setColorDepth(lgfx::rgb565_2Byte);
        void *buf = spr->createSprite(320, 240);
        if (!buf) {
            ESP_LOGE(TAG, "createSprite falhou");
            delete spr;
            return;
        }
        face_debug_set_sprite(spr);
        ESP_LOGI(TAG, "sprite interno criado em PSRAM 320x240 buf=%p", buf);
    }

    xTaskCreatePinnedToCore(serial_task, "FaceDbgTask", 4096, nullptr, 3, nullptr, 0);
    ESP_LOGI(TAG, "FaceDbgTask iniciada no Core 0");
}
