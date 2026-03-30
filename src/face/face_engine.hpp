#pragma once

#include <LovyanGFX.hpp>
#include "face_engine.h"
#include "face_renderer.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

/*
 * FaceEngine — motor de renderização facial com double-buffer e interpolação.
 *
 * Arquitetura:
 *   _drawBuf  → sprite ativo de escrita (frame atual)
 *   _frontBuf → sprite no display (sem rasgo visual)
 *   Após cada frame: display_push_sprite(_drawBuf) → std::swap()
 *
 * Ambos os sprites alocam ~150 KB em PSRAM (320×240×RGB565).
 * FaceRenderTask roda no Core 1, prioridade 20, loop 50 ms (20 Hz).
 *
 * Interpolação de expressões:
 *   applyParams(target) captura _params (estado atual renderizado) como _src,
 *   define _dst = target e inicia transição de _dst.transition_ms ms.
 *   A cada frame, renderLoop() interpola _src→_dst com cubic ease-in-out
 *   e atualiza _params via spinlock antes de renderizar.
 */
class FaceEngine {
public:
    static FaceEngine &instance();

    void init(void);
    void startTask(void);

    /*
     * Define a próxima expressão-alvo com interpolação suave.
     * Thread-safe (spinlock). Pode ser chamado de qualquer task/core.
     * A duração da transição é lida de target->transition_ms.
     * Se transition_ms == 0, a troca é instantânea no próximo frame.
     */
    void applyParams(const face_params_t *target);

    /* Retorna a expressão-alvo atual (_dst). Thread-safe. */
    void getTarget(face_params_t *out);

    /* Atualiza posição de olhar (x, y em [-0.8, 0.8]). Thread-safe. */
    void setGaze(float x, float y);

    /* Override visual de blink aplicado só no frame renderizado. */
    void setBlink(float amount);

private:
    static constexpr int FB_W = 320;
    static constexpr int FB_H = 240;
    static constexpr float DRIFT_X_HZ = 0.28f;
    static constexpr float DRIFT_Y_HZ = 0.22f;
    static constexpr float DRIFT_X_PX = 1.5f;
    static constexpr float DRIFT_Y_PX = 1.0f;
    static constexpr float MICRO_X_PX = 0.8f;
    static constexpr float MICRO_Y_PX = 0.6f;

    lgfx::LGFX_Sprite *_drawBuf  = nullptr;
    lgfx::LGFX_Sprite *_frontBuf = nullptr;
    TaskHandle_t       _task     = nullptr;
    bool               _initialized = false;

    FaceRenderer  _renderer;

    /* Estado interpolado atual — escrito pelo render task, lido por applyParams */
    face_params_t _params     = FACE_NEUTRAL;

    /* Transição: _src → _dst em _transDur ms a partir de _transStart */
    face_params_t _src        = FACE_NEUTRAL;
    face_params_t _dst        = FACE_NEUTRAL;
    uint32_t      _transStart = 0;
    uint32_t      _transDur   = 0;   /* 0 = instantâneo */

    portMUX_TYPE  _paramMux = portMUX_INITIALIZER_UNLOCKED;

    /* Posição de olhar do GazeService — atualizada via setGaze(), lida no renderLoop */
    volatile float _gaze_x = 0.0f;
    volatile float _gaze_y = 0.0f;
    portMUX_TYPE   _gazeMux = portMUX_INITIALIZER_UNLOCKED;

    volatile float _blink = 0.0f;
    portMUX_TYPE   _blinkMux = portMUX_INITIALIZER_UNLOCKED;

    /* E18: moduladores sutis de runtime por cima da face base */
    float    _micro_x            = 0.0f;
    float    _micro_y            = 0.0f;
    uint32_t _nextMicroUpdateMs  = 0;

    uint32_t _frameCount   = 0;
    uint32_t _fpsTimestamp = 0;

    static void sRenderTask(void *arg);
    bool initSprite(lgfx::LGFX_Sprite *&sprite, const char *label);
    void resetBuffers(void);
    void renderLoop(void);
    void drawFrame(const face_params_t &p, int dx = 0, int dy = 0);

    /* Interpolação linear de todos os campos de face_params_t */
    static face_params_t interpParams(const face_params_t &a,
                                      const face_params_t &b,
                                      float t);
};
