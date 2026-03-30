# E03 - Bring-up: Display ST7789

- Status: ✅ Critérios atendidos
- Complexidade: Média
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P1
- Risco: MÉDIO
- Depende de: E02

## Bring-up: Display ST7789

Complexidade: Média
Depende de: E02
Grupo: Bring-up HW
HW Real: SIM
ID: E03
Prioridade: P1
Risco: MÉDIO
Status: ✅ Critérios atendidos

## Objetivo

LovyanGFX inicializado com ST7789. Renderizando cor sólida e texto básico. FPS máximo medido com e sem Sprite. **Biblioteca usada: lovyan03/LovyanGFX.**

## Por que LovyanGFX e não o driver nativo

- DMA gerenciado automaticamente pela biblioteca
- Sprite nativo = double-buffering sem escrever código de swap manual
- API muito mais simples para shapes, fontes e transformações
- Suporte oficial a ST7789 + ESP32-S3 com config de pinos direto no código
- Elimina toda a camada de st7789_driver.c — LovyanGFX **é** o driver

## O que implementar

- Adicionar LovyanGFX como componente ESP-IDF (via idf_component.yml ou CMake)
- Criar `src/drivers/display/lgfx_config.hpp` com a config de hardware do ST7789
- Instanciar `LGFX` globalmente e expor via `src/drivers/display/display.h` com interface C-friendly
- Testar: `display_fill_color(COLOR_GREEN)`, `display_draw_text(10, 10, "OK")`
- Medir FPS com Sprite vs. sem Sprite (pushSprite vs. fillScreen direto)
- Confirmar que o microSD onboard em SDMMC não interfere na operação do display

## O que NÃO entra

- Sprite de double-buffering (vem na E16 — aqui só validar o driver base)
- Qualquer lógica de face ou parâmetros
- Integração com microSD nesta etapa

## Critérios de pronto

- `display_fill_color()` pinta a tela inteira sem artefatos
- FPS máximo documentado (ex: "45fps fill, 30fps com texto, 240×320 SPI 80MHz")
- LovyanGFX compilando como componente ESP-IDF sem warnings
- microSD onboard montado: sem interferência perceptível no display durante fill

## Testes mínimos

- Preencher tela com 5 cores diferentes — sem artefatos ou linhas fantasmas
- Montar o microSD onboard e chamar fill repetidamente — verificar ausência de interferência
- Medir FPS: `TickType_t t0 = xTaskGetTickCount(); for(int i=0;i<100;i++) display_fill(WHITE); float fps = 100.0f / ((xTaskGetTickCount()-t0)/1000.0f);`

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, ESP-IDF v5.x, LovyanGFX como componente, ST7789 240×320.
Pinos conforme hal_init.h: SPI_MOSI, SPI_MISO, SPI_CLK, ST7789_CS, ST7789_DC, ST7789_RST, ST7789_BL.

Tarefa A: lgfx_config.hpp — configurar LGFX_Device para ST7789 com os pinos acima.
Exemplo de estrutura:
  class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
  public:
    LGFX(void) {
      // config de bus: pin_mosi, pin_miso, pin_sclk, pin_dc, freq_write=80MHz
      // config de panel: pin_cs, pin_rst, pin_busy=-1, offset_rotation=0
    }
  };

Tarefa B: display.h/.c — wrapper C-friendly sobre o LGFX:
  void display_init(void);
  void display_fill_color(uint16_t color);
  void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void display_push_sprite(lgfx::LGFX_Sprite *sprite, int16_t x, int16_t y);

Tarefa C: como adicionar LovyanGFX ao idf_component.yml:
  dependencies:
    lovyan03/lovyangfx: "^1.1.0"

Saída: lgfx_config.hpp + display.h/.c + idf_component.yml atualizado + teste de FPS.
```

## ◎ Prompt de Revisão

```
Display com LovyanGFX da E03.
Verificar: (1) LGFX instanciado uma única vez (singleton — não criar múltiplos)? (2) SPI freq definida explicitamente (não deixar default)? (3) wrapper C em display.h (não expor LGFX diretamente no .h para código C)? (4) FPS medido e documentado? (5) SD não interfere?
Listar problemas.
```

## ✎ Prompt de Correção

```
Display com LovyanGFX com problema: [sintoma — ex: tela fica branca após init, ou FPS muito baixo]
Contexto: E03, ESP32-S3, LovyanGFX, ST7789.
Verificar: pin_dc vs pin_cs invertidos, freq_write, panel offset_rotation.
Saída: causa provável + config corrigida no lgfx_config.hpp.
```

## → Prompt de Continuidade

```
E03 concluída. LovyanGFX inicializado, ST7789 renderizando, FPS máximo documentado.
Próxima: E04 (bring-up de servos SCS0009).
Mostre o protocolo UART Feetech para SCS0009: como enviar posição (0–1023), ler posição e ler corrente. UART1 conforme hal_init.h.
```


