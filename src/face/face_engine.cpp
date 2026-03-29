#include "face_engine.hpp"
#include "display.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "watchdog_manager.h"

#include <utility>   /* std::swap */
#include <string.h>  /* memcpy   */
#include <cmath>     /* sinf      */

static const char *TAG = "FACE";

/* ── Helpers de interpolação ─────────────────────────────────────────────── */

static inline int8_t lerp8(int8_t a, int8_t b, float t)
{
    return (int8_t)((float)a + ((float)b - (float)a) * t + 0.5f);
}

static inline float lerpF(float a, float b, float t)
{
    return a + (b - a) * t;
}

static inline uint8_t lerpu8(uint8_t a, uint8_t b, float t)
{
    return (uint8_t)((float)a + ((float)b - (float)a) * t + 0.5f);
}

static inline uint16_t lerpRGB565(uint16_t a, uint16_t b, float t)
{
    const int ra = (a >> 11) & 0x1F, rb5 = (b >> 11) & 0x1F;
    const int ga = (a >>  5) & 0x3F, gb6 = (b >>  5) & 0x3F;
    const int ba = (a      ) & 0x1F, bb5 = (b      ) & 0x1F;
    const int r = ra + (int)(((float)(rb5 - ra)) * t + 0.5f);
    const int g = ga + (int)(((float)(gb6 - ga)) * t + 0.5f);
    const int bl = ba + (int)(((float)(bb5 - ba)) * t + 0.5f);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

static inline bool params_need_runtime_floor(const face_params_t &p)
{
    return (p.open_l <= 0.24f) && (p.open_r <= 0.24f);
}

static inline face_params_t apply_runtime_floor(face_params_t p)
{
    if (p.open_l < 0.34f) p.open_l = 0.34f;
    if (p.open_r < 0.34f) p.open_r = 0.34f;
    if (p.x_off < 108u) p.x_off = 108u;
    return p;
}

static void draw_alert_overlay(lgfx::LGFX_Sprite *spr, uint32_t now_ms)
{
    if (spr == nullptr) {
        return;
    }

    if (((now_ms / 260u) % 3u) == 2u) {
        return;
    }

    static constexpr uint16_t ALERT_COLOR = FACE_EYE_COLOR;
    static constexpr int CX = 282;
    static constexpr int TOP = 18;

    spr->drawLine(CX, TOP, CX - 10, TOP + 18, ALERT_COLOR);
    spr->drawLine(CX, TOP, CX + 10, TOP + 18, ALERT_COLOR);
    spr->drawLine(CX - 10, TOP + 18, CX + 10, TOP + 18, ALERT_COLOR);
    spr->fillRect(CX - 1, TOP + 6, 3, 7, ALERT_COLOR);
    spr->fillRect(CX - 1, TOP + 15, 3, 3, ALERT_COLOR);
}

static void draw_sleep_overlay(lgfx::LGFX_Sprite *spr, uint32_t now_ms)
{
    if (spr == nullptr) {
        return;
    }

    static constexpr uint16_t SLEEP_COLOR = FACE_EYE_COLOR;
    const int phase = (int)((now_ms / 180u) % 24u);
    const int bob0 = 104 - phase;
    const int bob1 = 82 - ((phase + 8) % 24);
    const int bob2 = 60 - ((phase + 16) % 24);

    auto draw_z = [spr](int x, int y, int w, int h, uint16_t color) {
        spr->drawLine(x, y, x + w, y, color);
        spr->drawLine(x + w, y, x, y + h, color);
        spr->drawLine(x, y + h, x + w, y + h, color);
    };

    if (phase < 18) {
        draw_z(238, bob0, 22, 12, SLEEP_COLOR);
    }
    if (phase >= 5 && phase < 22) {
        draw_z(258, bob1, 16, 9, SLEEP_COLOR);
    }
    if (phase >= 11) {
        draw_z(274, bob2, 10, 6, SLEEP_COLOR);
    }
}


/* ── FaceEngine::interpParams ────────────────────────────────────────────── */
face_params_t FaceEngine::interpParams(const face_params_t &a,
                                        const face_params_t &b,
                                        float t)
{
    face_params_t r;
    r.tl_l = lerp8(a.tl_l, b.tl_l, t);
    r.tr_l = lerp8(a.tr_l, b.tr_l, t);
    r.bl_l = lerp8(a.bl_l, b.bl_l, t);
    r.br_l = lerp8(a.br_l, b.br_l, t);
    r.tl_r = lerp8(a.tl_r, b.tl_r, t);
    r.tr_r = lerp8(a.tr_r, b.tr_r, t);
    r.bl_r = lerp8(a.bl_r, b.bl_r, t);
    r.br_r = lerp8(a.br_r, b.br_r, t);
    r.open_l    = lerpF(a.open_l, b.open_l, t);
    r.open_r    = lerpF(a.open_r, b.open_r, t);
    r.y_l       = lerp8(a.y_l, b.y_l, t);
    r.y_r       = lerp8(a.y_r, b.y_r, t);
    r.x_off     = lerpu8(a.x_off, b.x_off, t);
    r.rt_top    = lerpu8(a.rt_top, b.rt_top, t);
    r.rb_bot    = lerpu8(a.rb_bot, b.rb_bot, t);
    r.cv_top    = lerp8(a.cv_top, b.cv_top, t);
    r.cv_bot    = lerp8(a.cv_bot, b.cv_bot, t);
    r.color     = lerpRGB565(a.color, b.color, t);
    /* Campos não interpolados — usa valor do destino */
    r.transition_ms = b.transition_ms;
    r.priority      = b.priority;
    return r;
}

/* ── singleton ───────────────────────────────────────────────────────────── */
FaceEngine &FaceEngine::instance()
{
    static FaceEngine s_inst;
    return s_inst;
}

/* ── init ────────────────────────────────────────────────────────────────── */
void FaceEngine::init(void)
{
    _drawBuf  = new lgfx::LGFX_Sprite();
    _frontBuf = new lgfx::LGFX_Sprite();

    for (auto *s : {_drawBuf, _frontBuf}) {
        s->setPsram(true);
        s->setColorDepth(lgfx::rgb565_2Byte);
        void *buf = s->createSprite(320, 240);   /* landscape 320×240 */
        if (!buf) {
            ESP_LOGE(TAG, "createSprite 320x240 falhou — PSRAM insuficiente?");
            return;
        }
    }

    _renderer.init(_drawBuf);

    _params     = FACE_NEUTRAL;
    _src        = FACE_NEUTRAL;
    _dst        = FACE_NEUTRAL;
    _transStart = 0;
    _transDur   = 0;

    _frameCount   = 0;
    _fpsTimestamp = (uint32_t)(esp_timer_get_time() / 1000ULL);

    ESP_LOGD(TAG, "init OK  drawBuf=%p  frontBuf=%p",
             (void *)_drawBuf, (void *)_frontBuf);
}

/* ── applyParams ─────────────────────────────────────────────────────────── */
/*
 * Captura o estado atual (_params) como origem da transição,
 * define destino e inicia contagem de tempo.
 * Thread-safe via spinlock — pode ser chamado de qualquer core/task.
 */
void FaceEngine::applyParams(const face_params_t *target)
{
    if (!target) return;
    const uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    taskENTER_CRITICAL(&_paramMux);
    _src        = _params;   /* snapshot do estado renderizado atual */
    _dst        = *target;
    _transStart = now;
    _transDur   = target->transition_ms;
    taskEXIT_CRITICAL(&_paramMux);
}

/* ── setGaze ─────────────────────────────────────────────────────────────── */
void FaceEngine::setGaze(float x, float y)
{
    taskENTER_CRITICAL(&_gazeMux);
    _gaze_x = x;
    _gaze_y = y;
    taskEXIT_CRITICAL(&_gazeMux);
}

void FaceEngine::setAlertOverlay(int enabled)
{
    taskENTER_CRITICAL(&_overlayMux);
    _alertOverlay = (enabled != 0);
    taskEXIT_CRITICAL(&_overlayMux);
}

void FaceEngine::setSleepOverlay(int enabled)
{
    taskENTER_CRITICAL(&_overlayMux);
    _sleepOverlay = (enabled != 0);
    taskEXIT_CRITICAL(&_overlayMux);
}

/* ── getTarget ───────────────────────────────────────────────────────────── */
void FaceEngine::getTarget(face_params_t *out)
{
    if (!out) return;
    taskENTER_CRITICAL(&_paramMux);
    *out = _dst;
    taskEXIT_CRITICAL(&_paramMux);
}

void FaceEngine::getCurrent(face_params_t *out)
{
    if (!out) return;
    taskENTER_CRITICAL(&_paramMux);
    *out = _params;
    taskEXIT_CRITICAL(&_paramMux);
}

/* ── drawFrame ───────────────────────────────────────────────────────────── */
void FaceEngine::drawFrame(const face_params_t &p, int dx, int dy)
{
    /* Re-vincula o renderer ao drawBuf correto após cada swap */
    _renderer.init(_drawBuf);
    _renderer.draw(p, dx, dy);
}

/* ── renderLoop ──────────────────────────────────────────────────────────── */
void FaceEngine::renderLoop(void)
{
    if (!_drawBuf || !_frontBuf) {
        ESP_LOGE(TAG, "sprites não inicializados — encerrando task");
        vTaskDelete(nullptr);
        return;
    }

    wdt_register_task(nullptr, WDT_TASK_TIMEOUT_MS);

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        /* 1. Captura estado de transição (spinlock) */
        face_params_t src, dst;
        uint32_t t_start, t_dur;
        taskENTER_CRITICAL(&_paramMux);
        src     = _src;
        dst     = _dst;
        t_start = _transStart;
        t_dur   = _transDur;
        taskEXIT_CRITICAL(&_paramMux);

        /* 2. Calcula estado interpolado */
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        const uint32_t elapsed = now_ms - t_start;
        face_params_t cur;

        if (t_dur == 0u || elapsed >= (uint32_t)t_dur) {
            cur = dst;
        } else {
            float t = (float)elapsed / (float)t_dur;
            /* Cubic ease-in-out: smoothstep 3t² − 2t³ */
            t = t * t * (3.0f - 2.0f * t);
            cur = interpParams(src, dst, t);

            if ((t_dur >= 120u) && params_need_runtime_floor(dst)) {
                cur = apply_runtime_floor(cur);
            }
        }

        /* 3. Publica estado interpolado (spinlock) para applyParams() */
        taskENTER_CRITICAL(&_paramMux);
        _params = cur;
        taskEXIT_CRITICAL(&_paramMux);

        /* 4. Micro-movimentos — oscilação senoidal suave nos olhos
         *   gaze_x: 0.3 Hz × 1.5 px   gaze_y: 0.2 Hz × 1.0 px (fase 1.2 rad) */
        const float t_sec   = (float)now_ms * 0.001f;
        const int   micro_x = (int)roundf(sinf(6.28318f * 0.3f * t_sec) * 1.5f);
        const int   micro_y = (int)roundf(sinf(6.28318f * 0.2f * t_sec + 1.2f) * 1.0f);

        /* 4b. Gaze — offset de olhar do GazeService (escala: ±0.92 → ~±30 px / ±16 px) */
        float gx, gy;
        taskENTER_CRITICAL(&_gazeMux);
        gx = _gaze_x;
        gy = _gaze_y;
        taskEXIT_CRITICAL(&_gazeMux);
        const int gaze_px_x = (int)roundf(gx * 32.0f);
        const int gaze_px_y = (int)roundf(gy * 17.0f);

        bool alert_overlay = false;
        bool sleep_overlay = false;
        taskENTER_CRITICAL(&_overlayMux);
        alert_overlay = _alertOverlay;
        sleep_overlay = _sleepOverlay;
        taskEXIT_CRITICAL(&_overlayMux);

        /* 5. Renderiza na drawBuf com micro-offset + gaze */
        drawFrame(cur, micro_x + gaze_px_x, micro_y + gaze_px_y);
        if (alert_overlay) {
            draw_alert_overlay(_drawBuf, now_ms);
        }
        if (sleep_overlay) {
            draw_sleep_overlay(_drawBuf, now_ms);
        }

        /* 5. Empurra para o display físico */
        display_push_sprite(_drawBuf, 0, 0);

        /* 6. Troca buffers */
        std::swap(_drawBuf, _frontBuf);

        /* 7. Alimenta WDT */
        wdt_feed(nullptr);

        /* 8. Estatísticas de FPS a cada 5 s */
        _frameCount++;
        if (now_ms - _fpsTimestamp >= 5000u) {
            const float fps = (float)_frameCount * 1000.f /
                              (float)(now_ms - _fpsTimestamp);
            ESP_LOGD(TAG, "fps=%.1f", fps);
            _frameCount   = 0;
            _fpsTimestamp = now_ms;
        }

        /* 9. Aguarda próximo slot de 50 ms (20 Hz) */
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(50));
    }
}

/* ── task entry ──────────────────────────────────────────────────────────── */
void FaceEngine::sRenderTask(void *arg)
{
    static_cast<FaceEngine *>(arg)->renderLoop();
}

void FaceEngine::startTask(void)
{
    xTaskCreatePinnedToCore(
        sRenderTask,
        "FaceRenderTask",
        8192,
        this,
        20,
        nullptr,
        1   /* Core 1 */
    );
    ESP_LOGD(TAG, "FaceRenderTask criada no Core 1");
}

/* ══════════════════════════════════════════════════════════════════════════
   Wrappers C (extern "C")
   ══════════════════════════════════════════════════════════════════════════ */

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

extern "C" void face_engine_get_target(face_params_t *out)
{
    FaceEngine::instance().getTarget(out);
}

extern "C" void face_engine_get_current(face_params_t *out)
{
    FaceEngine::instance().getCurrent(out);
}

extern "C" void face_engine_set_gaze(float x, float y)
{
    FaceEngine::instance().setGaze(x, y);
}

extern "C" void face_engine_set_alert_overlay(int enabled)
{
    FaceEngine::instance().setAlertOverlay(enabled);
}

extern "C" void face_engine_set_sleep_overlay(int enabled)
{
    FaceEngine::instance().setSleepOverlay(enabled);
}



