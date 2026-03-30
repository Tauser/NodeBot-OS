# E16 - Face Loop Básico — Framebuffer + DMA 20fps

- Status: ✅ Critérios atendidos
- Complexidade: Alta
- Grupo: Face
- HW Real: SIM
- Notas: fps=20.0 estável. Placeholder verde com olhos e boca visível na tela. Design Sprint pendente antes de E17.
- Prioridade: P1
- Risco: MÉDIO
- Depende de: 

## Face Loop Básico — Framebuffer + DMA 20fps

Complexidade: Alta
Grupo: Face
HW Real: SIM
ID: E16
Notas: fps=20.0 estável. Placeholder verde com olhos e boca visível na tela. Design Sprint pendente antes de E17.
Prioridade: P1
Risco: MÉDIO
Status: ✅ Critérios atendidos

> 📍 **E16** · Grupo: Face · Prioridade: P1 · Depende de: E03 (LovyanGFX rodando)
> 

---

> ⚠️ **Erro comum que o Claude comete nesta etapa:**
> 

> - `face_engine` fica em `drivers/display/` — **errado**, deve ir em `src/face/`
> 

> - `event_bus`, `config_manager`, `log_manager`, `watchdog_manager` ficam em `drivers/` — **errado**, devem ir em `src/core/`
> 

> ⚠️ **Erro comum que o Claude comete nesta etapa:** colocar `face_engine` dentro de `drivers/display/` e colocar `event_bus/`, `config_manager/`, `log_manager/`, `watchdog_manager/` dentro de `drivers/`. Isso está errado. Se acontecer, use o prompt de correção abaixo antes de continuar.
> 

> **Regra:** `drivers/` = só código que fala diretamente com hardware via SPI/I2C/UART/I2S/RMT. Se qualquer outro módulo aparecer lá, use o prompt de correção abaixo **antes de continuar**.
> 

---

## O que é esta etapa

Fazer o display renderizar a 20fps com double-buffer. Nada de olho, expressão ou parâmetro ainda — só o loop de render funcionando.

## Quando está pronta

- [ ]  FPS ≥ 18 medido por 5 minutos contínuos
- [ ]  CPU da FaceRenderTask ≤ 25%
- [ ]  Zero tearing visual

---

## PASSO 1 — Cole este prompt no Claude (nova conversa)

```
Projeto: robô desktop ESP32-S3, 512KB SRAM, 8MB PSRAM, ST7789 240x320, LovyanGFX.
Core 1 = face. Sem malloc em hot paths.

Tarefa: implementar face loop com LGFX_Sprite double-buffer a 20fps.

[COLE AQUI o conteúdo do seu lgfx_config.hpp da E03]

Criar:
  face_engine.hpp  — lógica C++
  face_engine.h    — interface C (init, start_task, apply_params stub)
  face_engine.c    — wrapper C

Estrutura do loop (a cada 50ms via vTaskDelayUntil, Core 1, P20):
  1. drawBuf->fillScreen(TFT_BLACK)
  2. drawBuf->fillCircle(120, 160, 40, TFT_WHITE)  // placeholder
  3. drawBuf->pushSprite(0, 0)
  4. std::swap(drawBuf, frontBuf)
  5. Logar FPS a cada 5s: [FACE] fps=XX.X

Regras:
  - sprites criados com setPsram(true) — nunca em SRAM
  - pushSprite no drawBuf, swap DEPOIS do push
  - vTaskDelayUntil (não vTaskDelay)
  - WDT alimentado a cada ciclo
```

---

## PASSO 2 — Quando o Claude responder

Revise com este prompt na mesma conversa:

```
Revisar face_engine da E16.
Verificar: (1) setPsram(true) nos dois sprites? (2) swap após pushSprite? (3) vTaskDelayUntil? (4) WDT alimentado?
Listar problemas.
```

---

## PASSO 3 — Testar no hardware

1. Flash e ligar
2. Verificar círculo branco no centro do display
3. Medir FPS no serial monitor — meta ≥ 18fps
4. Deixar rodar 5 minutos — verificar que não crasha

---

## PASSO 4 — Quando os critérios de pronto estiverem OK

**Não abrir E17 ainda.**

Abrir a página 👉 **🎨 Design Sprint — Estilo Visual** e seguir as instruções de lá.

Só depois do Design Sprint ir para E17.

---

## Se o Claude colocar os arquivos no lugar errado

```
A estrutura de pastas está incorreta. Mover:

  src/drivers/display/face_engine.hpp → src/face/face_engine.hpp
  src/drivers/display/face_engine.h   → src/face/face_engine.h
  src/drivers/display/face_engine.cpp → src/face/face_engine.cpp

  src/drivers/event_bus/        → src/core/event_bus/
  src/drivers/config_manager/   → src/core/config_manager/
  src/drivers/log_manager/      → src/core/log_manager/
  src/drivers/watchdog_manager/ → src/core/watchdog_manager/

Regra: drivers/ contém SOMENTE código que fala diretamente com hardware via SPI/I2C/UART/I2S/RMT.
Infraestrutura de software vai em core/. Face engine vai em face/.

Ajustar CMakeLists.txt de todas as pastas afetadas.
Mostrar CMakeLists.txt raiz atualizado.
```

---

## Se der problema de FPS

```
FPS abaixo de 18 com LovyanGFX Sprite.
Contexto: E16, ESP32-S3, PSRAM, ST7789.
Verificar: freq_write do SPI no lgfx_config, se sprites estão em PSRAM (não SRAM).
Causa + config corrigida.
```


