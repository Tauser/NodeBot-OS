# LovyanGFX — Referencia para o Projeto

Biblioteca: lovyan03/LovyanGFX — substitui completamente o driver nativo ST7789.

---

## Por que LovyanGFX

| Aspecto          | Driver nativo                      | LovyanGFX                           |
| ---------------- | ---------------------------------- | ----------------------------------- |
| Double-buffering | Manual: 2 framebuffers + DMA swap  | LGFX_Sprite nativo com pushSprite() |
| DMA              | Configurar manualmente             | Gerenciado automaticamente          |
| Shapes           | Implementar fillCircle/fillEllipse | Prontos na API                      |
| Config           | Codigo de init SPI verboso         | Uma struct declarativa              |

---

## Instalacao como componente ESP-IDF

### idf_component.yml (recomendado)

```yaml
dependencies:
  lovyan03/lovyangfx:
    version: "^1.1.0"
```

Depois: `idf.py update-dependencies`

### CMake manual

```bash
git submodule add https://github.com/lovyan03/LovyanGFX.git components/LovyanGFX
```

---

## Configuracao do hardware — lgfx_config.hpp

```cpp
// src/drivers/display/lgfx_config.hpp
#pragma once
#include <LovyanGFX.hpp>
#include "hal_init.h"  // pinos definidos aqui

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;

public:
  LGFX(void) {
    // --- Bus (SPI) ---
    {
      auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;      // HSPI
      cfg.spi_mode   = 0;
      cfg.freq_write = 80000000;       // 80 MHz write
      cfg.freq_read  = 20000000;       // 20 MHz read
      cfg.pin_sclk   = SPI_CLK;
      cfg.pin_mosi   = SPI_MOSI;
      cfg.pin_miso   = SPI_MISO;
      cfg.pin_dc     = ST7789_DC;
      cfg.use_lock   = true;
      cfg.dma_channel= SPI_DMA_CH_AUTO;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    // --- Panel (ST7789) ---
    {
      auto cfg = _panel.config();
      cfg.pin_cs          = ST7789_CS;
      cfg.pin_rst         = ST7789_RST;
      cfg.pin_busy        = -1;
      cfg.memory_width    = 240;
      cfg.memory_height   = 320;
      cfg.panel_width     = 240;
      cfg.panel_height    = 320;
      cfg.offset_x        = 0;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;         // ajustar se tela aparecer rotacionada
      cfg.dummy_read_pixel= 8;
      cfg.dummy_read_bits = 1;
      cfg.readable        = false;
      cfg.invert          = true;      // ST7789 normalmente precisa de invert
      cfg.rgb_order       = false;
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = true;      // SPI compartilhado com SD
      _panel.config(cfg);
    }
    // --- Backlight ---
    {
      auto cfg = _light.config();
      cfg.pin_bl      = ST7789_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

extern LGFX lcd;  // instancia global — definida em display.cpp
```

---

## Wrapper C — display.h

```c
// src/drivers/display/display.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void    display_init(void);
void    display_fill_color(uint16_t color);
void    display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void    display_set_brightness(uint8_t level);  // 0-255
int16_t display_width(void);
int16_t display_height(void);

#ifdef __cplusplus
}
#endif
```

---

## Double-buffering com LGFX_Sprite

```cpp
// face_engine.cpp — padrao correto
#include "lgfx_config.hpp"

static LGFX_Sprite spriteA(&lcd);
static LGFX_Sprite spriteB(&lcd);
static LGFX_Sprite *drawBuf  = &spriteA;  // renderizar aqui
static LGFX_Sprite *frontBuf = &spriteB;  // exibir este

void face_engine_init(void) {
  lcd.init();
  lcd.setRotation(0);
  lcd.setBrightness(200);

  spriteA.setPsram(true);          // CRITICO: alocar em PSRAM
  spriteA.setColorDepth(16);       // RGB565 — 2 bytes por pixel
  spriteA.createSprite(240, 320);  // 240x320x2 = ~150KB em PSRAM

  spriteB.setPsram(true);
  spriteB.setColorDepth(16);
  spriteB.createSprite(240, 320);
}

void face_render_frame(const face_params_t *p) {
  // 1. Renderizar no buffer inativo
  drawBuf->fillScreen(TFT_BLACK);
  face_draw_eye(drawBuf, p, 120, 160);

  // 2. Enviar para o display via DMA
  drawBuf->pushSprite(0, 0);

  // 3. Trocar buffers
  std::swap(drawBuf, frontBuf);
}
```

---

## Desenhando o olho procedural

```cpp
void face_draw_eye(LGFX_Sprite *s, const face_params_t *p, int16_t cx, int16_t cy) {
  // Palpebra: elipse que abre/fecha
  int16_t ew = 80;
  int16_t eh = (int16_t)(80.0f * p->eyelid_l);
  if (eh < 4) eh = 4;
  s->fillEllipse(cx, cy, ew/2, eh/2, TFT_WHITE);

  // Highlight com gaze offset
  int16_t hx = cx + (int16_t)(p->gaze_x * 10.0f);
  int16_t hy = cy + (int16_t)(p->gaze_y * 8.0f);
  hx = std::max((int16_t)(cx - ew/2 + 8), std::min(hx, (int16_t)(cx + ew/2 - 8)));
  hy = std::max((int16_t)(cy - eh/3), std::min(hy, (int16_t)(cy + eh/3)));
  s->fillCircle(hx, hy, 4, TFT_WHITE);
}
```

---

## SPI compartilhado com microSD

```cpp
// bus_shared = true na config libera o bus entre frames
// SD usa StorageFlushTask (P5) — nunca dentro de FaceRenderTask
// Nunca chamar sd_write_file() dentro do FaceRenderTask
```

---

## Referencia rapida de metodos

| Metodo                            | Uso                         |
| --------------------------------- | --------------------------- |
| lcd.init()                        | Inicializar display         |
| lcd.setBrightness(200)            | Brilho 0-255                |
| lcd.setRotation(0..3)             | Rotacao da tela             |
| sprite.createSprite(w, h)         | Alocar framebuffer          |
| sprite.setPsram(true)             | Forcar alocacao em PSRAM    |
| sprite.fillScreen(color)          | Limpar sprite               |
| sprite.fillCircle(x,y,r,c)        | Circulo preenchido          |
| sprite.fillEllipse(x,y,rx,ry,c)   | Elipse preenchida           |
| sprite.fillRoundRect(x,y,w,h,r,c) | Retangulo arredondado       |
| sprite.drawString(s, x, y)        | Texto                       |
| sprite.setTextColor(c)            | Cor do texto                |
| sprite.pushSprite(x, y)           | Enviar para display via DMA |
| sprite.pushSprite(x,y,transp)     | Enviar com transparencia    |

---

## Cores padrao do projeto

```cpp
#define FACE_BG        TFT_BLACK   // fundo
#define FACE_EYE       TFT_WHITE   // branco do olho
#define FACE_HIGHLIGHT TFT_WHITE   // brilho do olho
#define UI_WARN        TFT_YELLOW  // avisos
#define UI_ERROR       TFT_RED     // erros
```

---

## Etapas afetadas

| Etapa | Impacto                                                |
| ----- | ------------------------------------------------------ |
| E03   | Driver e LovyanGFX — sem st7789_driver.c nativo        |
| E16   | Double-buffer via LGFX_Sprite (nao framebuffer manual) |
| E17   | Renderizacao usa fillEllipse/fillCircle do Sprite      |
| E18   | Micro-movimentos: somar offset ao desenho no Sprite    |
| E19   | EmotionMapper chama face_draw_eye com params variados  |
| E38   | DiagnosticMode pode usar Sprite sobreposto para texto  |
