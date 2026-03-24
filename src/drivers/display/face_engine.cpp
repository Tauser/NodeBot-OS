#include "face_engine.hpp"
#include "display.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <utility>   /* std::swap */
#include <string.h>  /* memcpy    */

static const char *TAG = "face_engine";

/* ── singleton ──────────────────────────────────────────────────────── */
FaceEngine &FaceEngine::instance()
{
    static FaceEngine s_inst;
    return s_inst;
}

/* ── init ────────────────────────────────────────────────────────────── */
void FaceEngine::init(void)
{
    /*
     * Cria dois sprites 240×320 RGB565 (~150 KB cada).
     * LovyanGFX::createSprite tenta PSRAM automaticamente quando
     * MALLOC_CAP_SPIRAM está disponível e CONFIG_SPIRAM=y.
     */
    _drawBuf  = new lgfx::LGFX_Sprite();
    _frontBuf = new lgfx::LGFX_Sprite();

    for (auto *s : {_drawBuf, _frontBuf}) {
        s->setColorDepth(lgfx::rgb565_2Byte);
        void *buf = s->createSprite(240, 320);
        if (!buf) {
            ESP_LOGE(TAG, "createSprite 240x320 falhou — PSRAM insuficiente?");
            return;
        }
    }

    /* Parâmetros padrão: olhos abertos, boca neutra */
    _params.eye_open    = 80;
    _params.mouth_curve = 50;
    _params.blink       = 0;

    _frameCount   = 0;
    _fpsTimestamp = (uint32_t)(esp_timer_get_time() / 1000ULL);

    ESP_LOGI(TAG, "init OK  drawBuf=%p  frontBuf=%p",
             (void *)_drawBuf, (void *)_frontBuf);
}

/* ── applyParams ─────────────────────────────────────────────────────── */
void FaceEngine::applyParams(const face_params_t *p)
{
    if (!p) return;
    taskENTER_CRITICAL(&_paramMux);
    memcpy(&_params, p, sizeof(_params));
    taskEXIT_CRITICAL(&_paramMux);
}

/* ── drawFrame ───────────────────────────────────────────────────────── */
void FaceEngine::drawFrame(const face_params_t &p)
{
    /* ── Fundo ────────────────────────────────────────────────────── */
    _drawBuf->fillScreen(TFT_BLACK);

    /* ── Cabeça ───────────────────────────────────────────────────── */
    _drawBuf->fillCircle(120, 160, 100, 0x2104u);   /* cinza escuro */
    _drawBuf->drawCircle(120, 160, 100, TFT_WHITE);

    /* ── Olhos ────────────────────────────────────────────────────── */
    if (p.blink) {
        /* piscada: linha horizontal */
        _drawBuf->drawFastHLine(85, 120, 20, TFT_WHITE);
        _drawBuf->drawFastHLine(115, 120, 20, TFT_WHITE);
    } else {
        /* abertura proporcional a eye_open (0-100 → raio 2-12) */
        int32_t eye_r = 2 + (int32_t)p.eye_open * 10 / 100;
        _drawBuf->fillCircle( 90, 120, eye_r, TFT_WHITE);
        _drawBuf->fillCircle(150, 120, eye_r, TFT_WHITE);
        /* pupila */
        _drawBuf->fillCircle( 92, 122, eye_r / 2 + 1, TFT_BLACK);
        _drawBuf->fillCircle(152, 122, eye_r / 2 + 1, TFT_BLACK);
    }

    /* ── Boca ─────────────────────────────────────────────────────── */
    /*
     * mouth_curve 50 = reta; >50 = sorriso (arco p/ baixo);
     * <50 = triste (arco p/ cima).
     */
    if (p.mouth_curve >= 50) {
        int32_t dy = (int32_t)(p.mouth_curve - 50u) / 5;
        _drawBuf->drawArc(120, 190 + dy, 28, 26, 200, 340, TFT_WHITE);
    } else {
        int32_t dy = (int32_t)(50u - p.mouth_curve) / 5;
        _drawBuf->drawArc(120, 220 - dy, 28, 26,  20, 160, TFT_WHITE);
    }
}

/* ── renderLoop ──────────────────────────────────────────────────────── */
void FaceEngine::renderLoop(void)
{
    if (!_drawBuf || !_frontBuf) {
        ESP_LOGE(TAG, "sprites não inicializados — encerrando task");
        vTaskDelete(nullptr);
        return;
    }

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        /* 1. Captura parâmetros de forma atômica */
        face_params_t params;
        taskENTER_CRITICAL(&_paramMux);
        memcpy(&params, &_params, sizeof(params));
        taskEXIT_CRITICAL(&_paramMux);

        /* 2. Renderiza na drawBuf */
        drawFrame(params);

        /* 3. Empurra drawBuf para o display físico */
        display_push_sprite(_drawBuf, 0, 0);

        /* 4. Troca buffers (frontBuf ← drawBuf atual) */
        std::swap(_drawBuf, _frontBuf);

        /* 5. Estatísticas de FPS a cada 5 s */
        _frameCount++;
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if (now_ms - _fpsTimestamp >= 5000u) {
            float fps = (float)_frameCount * 1000.f /
                        (float)(now_ms - _fpsTimestamp);
            ESP_LOGI(TAG, "FPS=%.1f  frames=%" PRIu32, fps, _frameCount);
            _frameCount   = 0;
            _fpsTimestamp = now_ms;
        }

        /* 6. Aguarda próximo slot de 50 ms (20 Hz) */
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(50));
    }
}

/* ── task entry ──────────────────────────────────────────────────────── */
void FaceEngine::sRenderTask(void *arg)
{
    static_cast<FaceEngine *>(arg)->renderLoop();
}

void FaceEngine::startTask(void)
{
    xTaskCreatePinnedToCore(
        sRenderTask,
        "FaceRenderTask",
        8192,          /* stack — suficiente para snprintf + drawArc */
        this,
        20,            /* prioridade 20 */
        nullptr,
        1              /* Core 1 */
    );
    ESP_LOGI(TAG, "FaceRenderTask criada no Core 1");
}

/* ══════════════════════════════════════════════════════════════════════
   Wrapper C (extern "C")
   ══════════════════════════════════════════════════════════════════════ */

extern "C" void face_engine_init(void)
{
    FaceEngine::instance().init();
}

extern "C" void face_engine_start_task(void)
{
    FaceEngine::instance().startTask();
}

extern "C" void face_engine_apply_params(const face_params_t *p)
{
    FaceEngine::instance().applyParams(p);
}
