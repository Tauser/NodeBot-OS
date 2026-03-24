#pragma once

#include <LovyanGFX.hpp>
#include "face_engine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

/*
 * FaceEngine — motor de renderização facial com double-buffer.
 *
 * Arquitetura:
 *   _drawBuf  → sprite ativo de escrita (renderização atual)
 *   _frontBuf → sprite no display (pode ser relido sem rasgo)
 *   Após cada frame: display_push_sprite(_drawBuf) → std::swap()
 *
 * Ambos os sprites alocam ~150 KB em PSRAM (240×320×RGB565).
 * FaceRenderTask roda no Core 1, prioridade 20, loop 50 ms (20 Hz).
 */
class FaceEngine {
public:
    static FaceEngine &instance();

    void init(void);
    void startTask(void);
    void applyParams(const face_params_t *p);

private:
    lgfx::LGFX_Sprite *_drawBuf  = nullptr;
    lgfx::LGFX_Sprite *_frontBuf = nullptr;

    face_params_t _params   = {};
    portMUX_TYPE  _paramMux = portMUX_INITIALIZER_UNLOCKED;

    uint32_t _frameCount   = 0;
    uint32_t _fpsTimestamp = 0;   /* ms (esp_timer) */

    static void sRenderTask(void *arg);
    void renderLoop(void);
    void drawFrame(const face_params_t &p);
};
