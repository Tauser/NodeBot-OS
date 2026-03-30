#include "face_engine.hpp"
#include "display.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "watchdog_manager.h"

#include <utility>   /* std::swap */
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

static inline float randf_unit(void)
{
    return (float)(esp_random() & 0xFFFFu) / 65535.0f;
}

static inline int8_t clamp8(int v, int lo, int hi)
{
    if (v < lo) return (int8_t)lo;
    if (v > hi) return (int8_t)hi;
    return (int8_t)v;
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
    r.gaze_x    = lerpF(a.gaze_x, b.gaze_x, t);
    r.gaze_y    = lerpF(a.gaze_y, b.gaze_y, t);
    r.squint_l  = lerpF(a.squint_l, b.squint_l, t);
    r.squint_r  = lerpF(a.squint_r, b.squint_r, t);
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

bool FaceEngine::initSprite(lgfx::LGFX_Sprite *&sprite, const char *label)
{
    sprite = new lgfx::LGFX_Sprite();
    if (!sprite) {
        ESP_LOGE(TAG, "falha ao alocar sprite %s", label);
        return false;
    }

    sprite->setPsram(true);
    sprite->setColorDepth(lgfx::rgb565_2Byte);

    void *buf = sprite->createSprite(FB_W, FB_H);
    if (!buf) {
        ESP_LOGE(TAG, "createSprite %s %dx%d falhou — PSRAM insuficiente?",
                 label, FB_W, FB_H);
        delete sprite;
        sprite = nullptr;
        return false;
    }

    sprite->fillScreen(TFT_BLACK);
    return true;
}

void FaceEngine::resetBuffers(void)
{
    if (_drawBuf) {
        _drawBuf->deleteSprite();
        delete _drawBuf;
        _drawBuf = nullptr;
    }

    if (_frontBuf) {
        _frontBuf->deleteSprite();
        delete _frontBuf;
        _frontBuf = nullptr;
    }

    _initialized = false;
}

/* ── init ────────────────────────────────────────────────────────────────── */
void FaceEngine::init(void)
{
    if (_initialized) {
        ESP_LOGW(TAG, "init ignorado: face_engine ja inicializado");
        return;
    }

    if (!initSprite(_drawBuf, "drawBuf") ||
        !initSprite(_frontBuf, "frontBuf")) {
        resetBuffers();
        return;
    }

    _frontBuf->fillScreen(TFT_BLACK);

    _renderer.init(_drawBuf);

    _params     = FACE_NEUTRAL;
    _src        = FACE_NEUTRAL;
    _dst        = FACE_NEUTRAL;
    _transStart = 0;
    _transDur   = 0;
    _micro_x    = 0.0f;
    _micro_y    = 0.0f;
    _nextMicroUpdateMs = 0;

    _frameCount   = 0;
    _fpsTimestamp = (uint32_t)(esp_timer_get_time() / 1000ULL);
    _initialized  = true;

    ESP_LOGI(TAG, "init OK  drawBuf=%p  frontBuf=%p",
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
    if (x < -1.0f) x = -1.0f;
    if (x >  1.0f) x =  1.0f;
    if (y < -1.0f) y = -1.0f;
    if (y >  1.0f) y =  1.0f;

    taskENTER_CRITICAL(&_gazeMux);
    _gaze_x = x;
    _gaze_y = y;
    taskEXIT_CRITICAL(&_gazeMux);
}

void FaceEngine::setBlink(float amount)
{
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;

    taskENTER_CRITICAL(&_blinkMux);
    _blink = amount;
    taskEXIT_CRITICAL(&_blinkMux);
}

/* ── getTarget ───────────────────────────────────────────────────────────── */
void FaceEngine::getTarget(face_params_t *out)
{
    if (!out) return;
    taskENTER_CRITICAL(&_paramMux);
    *out = _dst;
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
        _task = nullptr;
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
        }

        /* 3. Publica estado interpolado (spinlock) para applyParams() */
        taskENTER_CRITICAL(&_paramMux);
        _params = cur;
        taskEXIT_CRITICAL(&_paramMux);

        /* 3b. Blink é override visual de runtime: não altera a face base salva. */
        float blink;
        taskENTER_CRITICAL(&_blinkMux);
        blink = _blink;
        taskEXIT_CRITICAL(&_blinkMux);
        if (blink > 0.0f) {
            cur.open_l = lerpF(cur.open_l, 0.03f, blink);
            cur.open_r = lerpF(cur.open_r, 0.03f, blink);

            cur.bl_l = lerp8(cur.bl_l, clamp8((int)cur.bl_l + 28, -40, 40), blink);
            cur.br_l = lerp8(cur.br_l, clamp8((int)cur.br_l + 28, -40, 40), blink);
            cur.bl_r = lerp8(cur.bl_r, clamp8((int)cur.bl_r + 28, -40, 40), blink);
            cur.br_r = lerp8(cur.br_r, clamp8((int)cur.br_r + 28, -40, 40), blink);
            cur.tl_l = lerp8(cur.tl_l, clamp8((int)cur.tl_l + 6, -40, 40), blink);
            cur.tr_l = lerp8(cur.tr_l, clamp8((int)cur.tr_l + 6, -40, 40), blink);
            cur.tl_r = lerp8(cur.tl_r, clamp8((int)cur.tl_r + 6, -40, 40), blink);
            cur.tr_r = lerp8(cur.tr_r, clamp8((int)cur.tr_r + 6, -40, 40), blink);
            cur.y_l  = lerp8(cur.y_l,  clamp8((int)cur.y_l - 4, -60, 60), blink);
            cur.y_r  = lerp8(cur.y_r,  clamp8((int)cur.y_r - 4, -60, 60), blink);
        }

        /* 4. E18 runtime: drift suave por cima da face base.
         * Pequenas oscilações contínuas dão vida sem redefinir a expressão. */
        const float t_sec   = (float)now_ms * 0.001f;
        const float drift_x = sinf(6.28318f * DRIFT_X_HZ * t_sec) * DRIFT_X_PX;
        const float drift_y = sinf(6.28318f * DRIFT_Y_HZ * t_sec + 1.2f) * DRIFT_Y_PX;

        /* 4b. Micro-movimentos discretos e suaves ao redor do gaze alvo.
         * Atualizados em baixa frequência para evitar vibração mecânica. */
        if (_nextMicroUpdateMs == 0u || now_ms >= _nextMicroUpdateMs) {
            _micro_x = (randf_unit() * 2.0f - 1.0f) * MICRO_X_PX;
            _micro_y = (randf_unit() * 2.0f - 1.0f) * MICRO_Y_PX;
            _nextMicroUpdateMs = now_ms + 180u + (esp_random() % 241u); /* 180–420 ms */
        }

        /* 4c. Gaze — offset principal de olhar vindo do runtime (GazeService). */
        float gx, gy;
        taskENTER_CRITICAL(&_gazeMux);
        gx = _gaze_x;
        gy = _gaze_y;
        taskEXIT_CRITICAL(&_gazeMux);
        const int gaze_px_x = (int)roundf(gx * 18.0f);
        const int gaze_px_y = (int)roundf(gy * 12.0f);

        /* 5. Renderiza com face base + gaze runtime + drift + micro offsets */
        const int runtime_dx = (int)roundf((float)gaze_px_x + drift_x + _micro_x);
        const int runtime_dy = (int)roundf((float)gaze_px_y + drift_y + _micro_y);
        drawFrame(cur, runtime_dx, runtime_dy);

        /* 6. Empurra para o display físico usando o buffer recém-renderizado */
        display_push_sprite(_drawBuf, 0, 0);

        /* 7. Troca buffers: o antigo front vira draw do próximo frame */
        std::swap(_drawBuf, _frontBuf);

        /* 8. Alimenta WDT */
        wdt_feed(nullptr);

        /* 9. Estatísticas de FPS a cada 5 s */
        _frameCount++;
        if (now_ms - _fpsTimestamp >= 5000u) {
            const float fps = (float)_frameCount * 1000.f /
                              (float)(now_ms - _fpsTimestamp);
            ESP_LOGI(TAG, "fps=%.1f", fps);
            _frameCount   = 0;
            _fpsTimestamp = now_ms;
        }

        /* 10. Aguarda próximo slot de 50 ms (20 Hz) */
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
    if (!_initialized) {
        ESP_LOGE(TAG, "startTask falhou: face_engine nao inicializado");
        return;
    }

    if (_task) {
        ESP_LOGW(TAG, "startTask ignorado: FaceRenderTask ja criada");
        return;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        sRenderTask,
        "FaceRenderTask",
        8192,
        this,
        20,
        &_task,
        1   /* Core 1 */
    );

    if (ok == pdPASS) {
        ESP_LOGI(TAG, "FaceRenderTask criada no Core 1");
    } else {
        _task = nullptr;
        ESP_LOGE(TAG, "falha ao criar FaceRenderTask");
    }
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

extern "C" void face_engine_set_gaze(float x, float y)
{
    FaceEngine::instance().setGaze(x, y);
}

extern "C" void face_engine_set_blink(float amount)
{
    FaceEngine::instance().setBlink(amount);
}
