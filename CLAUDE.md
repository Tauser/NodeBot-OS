# CLAUDE.md — noisebot · Robô Desktop ESP32-S3

> Leia este arquivo antes de qualquer sessão. Todo o conteúdo está local — sem Notion.
> Atualize a seção **Estado Atual** ao final de cada etapa.
>
> Documentação detalhada:
> - `docs/LOVYANGFX.md`        — config completa, double-buffer, shapes, API
> - `docs/FE_TTLINKER.md`      — servos SCS0009, protocolo Feetech, driver UART
> - `docs/ETAPAS_CRITICAS.md`  — E04, E23, E25, E33, E39 com código de proteção
> - `docs/ETAPAS.md`           — índice principal das etapas; abrir só o `.md` da etapa relevante em `docs/etapas/`
> - `docs/EMO_SPEC.md`         — referência de emoção/comportamento/face; consultar apenas quando o trabalho envolver face, idle, gaze, emoção, persona ou behavior

---

## 🤖 O Projeto

Robô desktop companion inspirado no EMO, com expressividade emocional, interação por voz
e operação offline-first sobre mesa.

**Plataforma:** ESP32-S3-WROOM CAM N16R8 · 512 KB SRAM · 8 MB PSRAM · 16 MB Flash

---

## 🔌 Hardware

| Componente    | Especificação                                                         |
|---------------|-----------------------------------------------------------------------|
| MCU           | ESP32-S3-WROOM CAM N16R8                                              |
| Display       | ST7789 2" 240x320 via SPI                                             |
| Camera        | OV2640 (onboard)                                                      |
| Microfone     | INMP441 via I2S                                                       |
| Speaker       | MAX98357A via I2S                                                     |
| Servos        | 2x SCS0009 via FE-TTLinker (UART full-duplex -> TTL half-duplex)      |
| IMU           | MPU-6050 ou ICM-42688 via I2C                                         |
| Touch         | Fita de cobre — 4 zonas                                               |
| LEDs          | 2x WS2812 via RMT                                                     |
| Bateria       | LiPo 1S 3000 mAh                                                      |
| Fuel gauge    | MAX17048 via I2C                                                      |
| Charger       | bq25185 via I2C                                                       |
| Boost         | TPS61088                                                              |
| microSD       | SDMMC 1-bit onboard (CMD=38, CLK=39, DATA0=40)                       |

---

## ⚙️ Arquitetura de Cores

| Core   | Responsabilidade                                 | Tasks (prioridade FreeRTOS)                                                              |
|--------|--------------------------------------------------|------------------------------------------------------------------------------------------|
| Core 0 | WiFi/BT stack, camera, wake word, cloud          | WiFi P23 · WakeNet P15 · Camera P12 · OTA P8                                            |
| Core 1 | Face render, behavior, audio I/O, sensores, safety | Safety P22 · FaceRender P20 · AudioIO P18 · EventDispatch P15 · Behavior P12 · Sensor P8 · Storage P5 |

---

## 🏗️ Regras de Arquitetura — NUNCA violar

1. **Sem malloc em hot paths** — pools pre-alocados em SRAM (render loop, behavior loop, audio task).
2. **EventBus entre modulos** — nunca chamadas diretas entre servicos.
3. **Face procedural EMO-style** — 20 fps, double-buffer via `LGFX_Sprite *draw_buf` em PSRAM. Nunca renderizar direto no LCD.
4. **Offline-first** — cloud e enhancement, nunca dependencia critica.
5. **MotionSafety obrigatorio** — todo movimento de servo passa por `motion_safety_check()` antes de executar.
6. **LovyanGFX e a unica lib de display** — driver nativo ST7789 esta banido.
7. **Wrapper C** — modulos de face exportam `face_renderer.h` em C puro para compatibilidade.

### Bibliotecas mandatorias

| Componente       | Biblioteca           | Observacao                          |
|------------------|----------------------|-------------------------------------|
| Display ST7789   | lovyan03/LovyanGFX   | DMA automatico, Sprite nativo       |
| Double-buffering | LGFX_Sprite          | Canvas de render; swap via DMA      |
| Servos SCS0009   | FE-TTLinker (hw)     | UART full-duplex normal no ESP32    |

---

## 📐 Orcamento de Recursos

| Recurso          | Total  | Meta de uso        |
|------------------|--------|--------------------|
| SRAM             | 512 KB | < 400 KB           |
| PSRAM            | 8 MB   | < 3 MB uso normal  |
| Flash (App A+B)  | 16 MB  | <= 6 MB + assets   |
| CPU Core 1 medio | —      | < 70%              |
| CPU Core 1 pico  | —      | < 85%              |
| Autonomia        | 3 Ah   | > 6h uso normal    |

> Se Core 1 > 80% pico: desabilitar CuriosityService + GestureService (+12% margem).

### Projecao de CPU por grupo

| Apos Etapa       | Medio | Pico  | Status    |
|------------------|-------|-------|-----------|
| E15 (Runtime)    | ~5%   | ~12%  | OK        |
| E16-E19 (Face)   | ~28%  | ~35%  | OK        |
| E28 (Audio)      | ~55%  | ~68%  | MONITORAR |
| E33 (Behavior)   | ~62%  | ~75%  | MONITORAR |
| E37 (Cloud)      | ~70%  | ~85%  | LIMITE    |

---

## 🚦 EventBus — Prioridades

| Nivel    | Exemplos de eventos                                        |
|----------|------------------------------------------------------------|
| SAFETY   | EVT_SERVO_BLOCKED, EVT_EMERGENCY_STOP                      |
| SYSTEM   | EVT_HEALTH_CHANGED, EVT_POWER_MODE_CHANGED                 |
| BEHAVIOR | EVT_WAKE_WORD, EVT_TOUCH_DETECTED, EVT_FACE_COMMAND        |
| COSMETIC | EVT_GAZE_UPDATE, EVT_LED_UPDATE                            |

---

## 🔋 Modos de Degradacao

```
NORMAL -> LOW_POWER (<20%) -> CRITICAL_POWER (<10%) -> SAFE_MODE (3x boot fail) -> SHUTDOWN (<3V ou >75C)
```

---

## ⏱️ Timeouts de Cloud

| Timeout          | Valor   | Acao                                |
|------------------|---------|-------------------------------------|
| T_response_local | 300 ms  | Resposta local sempre disponivel    |
| T_cloud_soft     | 600 ms  | Inicia fallback local como primario |
| T_cloud_hard     | 1200 ms | Aborta request cloud, loga timeout  |

---

## 💡 Indicadores de LED (nao desabilitaveis)

| Estado                | LED               |
|-----------------------|-------------------|
| Normal                | Verde             |
| Degraded              | Ambar             |
| Escutando (mic ativo) | Vermelho fixo     |
| Camera ativa          | Vermelho piscante |
| Privacy mode          | Branco fixo       |

---

## 🎯 Politica de Prioridades de Face

```
P1 SAFETY > P2 SYSTEM > P3 DIALOGUE > P4 ATTENTION > P5 ROUTINE > P6 MOOD > P7 COSMETIC
```

---

## 🗂️ Mapa das 43 Etapas

| Grupo             | Etapas  | Objetivo                                                        |
|-------------------|---------|-----------------------------------------------------------------|
| Fundacao          | E01-E02 | Toolchain, pinagem congelada                                    |
| Bring-up HW       | E03-E11 | Um periferico por etapa + validacao de consumo                  |
| Runtime           | E12-E15 | EventBus, Config, Storage/Log, Watchdog                         |
| Face              | E16-E19 | Face loop -> Design Sprint -> params, micro-mov, EmotionMapper  |
| Gaze + Idle       | E20-E22 | Saccades, state vector, bocejo                                  |
| Safety            | E23-E25 | MotionSafety, HealthMonitor, Brownout/SafeMode                  |
| Interacao         | E26-E27 | TouchService, IMUService                                        |
| Audio             | E28-E32 | Captura/VAD, Playback, Wake word, Intents, TTS                  |
| Comportamento     | E33-E34 | BehaviorEngine, Gestos + soak 8h                                |
| Personagem        | E35     | Memoria, Mood, Atencao, Engajamento                             |
| Camera/Cloud      | E36-E37 | CameraService, CloudBridge                                      |
| Produto           | E38-E41 | Observabilidade, OTA, Factory, Jig                              |

### Sequencia obrigatoria

```
E01 -> E02 -> E03..E11 (em ordem, um por vez)
                 |
           E12 -> E13 -> E14 -> E15
                 |
      E16 -> DESIGN SPRINT -> E17 -> E18 -> E19
                 |
           E20 -> E21 -> E22
                 |
      E23 -> E24 -> E25   <- SAFETY (obrigatorio antes de servos)
                 |
           E26 -> E27
                 |
      E28 -> E29 -> E30 -> E31 -> E32
                 |
           E33 -> E34
                 |
               E35
                 |
      E36 (camera)   E37 (cloud)   <- podem ser paralelos
                 |
      E38 -> E39 -> E40 -> E41
```

> CRITICO: E23 (MotionSafety) deve ser implementado ANTES de qualquer integracao de servo com software.

---

## ⚠️ Etapas de Risco Critico — resumo

Detalhes completos com codigo em `docs/ETAPAS_CRITICAS.md`.

- **E04** — Bring-up Servos: overcurrent >900mA -> para automaticamente
- **E23** — MotionSafetyService: P22, Core 1, 5ms, sem malloc, sem delay, sem I/O SD
- **E25** — Brownout handler tem apenas ~5ms — somente flush + NVS commit
- **E33** — BehaviorLoop alimenta heartbeat ANTES de qualquer outra coisa
- **E39** — OTA: verificar assinatura ECDSA ANTES do swap de particao

---

## 📍 Estado Atual

```
Etapa em andamento : Soak 8h + gravar templates KWS + validacao integrada
Ultima concluida   : E43 - Face Tracking com servo de pescoco (PID pan + safety)
Todas as 43 etapas : IMPLEMENTADAS (107 commits)
Proxima pendencia  : Soak 8h todas as tasks ativas + re-gravar templates KWS
Branch git         : main
```

### Decisoes acumuladas E28–E43 + post-E04 que afetam manutencao

**Audio / Pipeline de voz**
- WakeWordService: AFE pipeline (afe_config_init "M") + WakeNet9 Jarvis, Core 0 P15, auto-supressão 800ms.
- Colisão de nome: `vad_process` do esp-sr conflita com o nosso — renomeado para `nb_vad_process`.
- esp-sr instalado via component manager: `main/idf_component.yml` + `managed_components/espressif__esp-sr`.
- TTS: frases PCM/WAV em /sdcard/tts/, tts_play_phrase(phrase_id_t) + tts_play_dynamic(TPL_HOUR, hora); publica EVT_TTS_DONE.
- DialogueStateService: FSM IDLE→LISTENING(6s)→PROCESSING(5s)→SPEAKING→IDLE; EVT_DIALOGUE_STATE_CHANGED a cada transição.
- dialogue_state_event_t{uint8 state} adicionado ao event_bus.h; led_router cancela LED_STATE_LISTENING via EVT_DIALOGUE_STATE_CHANGED.
- wake_word_suppress_ms(ms) + vad_suppress_ms(ms) chamados pelo tts_task antes de audio_play_pcm.
- IntentMapper: P13, timer 250ms pós-WHOOSH antes de capturar (evita eco speaker), onset pad 50ms (800 samples), RMS² threshold 500 (~-36 dBFS).
- KeywordSpotter: MFCC C1..C12 (omite C0 — sensível a ganho), Subsequence DTW sem banda, threshold 120.0 TEMPORARIO (re-gravar templates). Templates: /sdcard/kws/{kw_name}_{0..4}.wav (16kHz mono 16-bit). Gravar via /sdcard/kws/.record trigger ou console serial.
- IntentMapper: EVT_INTENT_DETECTED com payload intent_event_t{uint8 intent, uint8 confidence}. 12 intents.

**Face**
- face_engine_register_events() separado de face_engine_start_task() — subscribe só após event_bus_init().
- face_renderer: squint direito espelhado (mirror=true); FACE_ANGRY squint 0.80→0.38.

**Comportamento**
- BehaviorEngine: BehaviorLoopTask Core 1 P12 100ms; ordem: heartbeat→state_vector_tick→eventos→BT→publish.
- StateVector 7 dimensões (energy, valence, arousal, social, attn, comfort, affinity) em PSRAM.
- FSM: SLEEP→IDLE→ENGAGED→TALKING→SAFE_MODE.

**Servo / Safety**
- SCS0009 via UART1 GPIO20(TX)/GPIO46(RX), 1 Mbps, FE-TTLinker. GPIO46 requer pull-up 4k7 externo.
- MotionSafety distingue overcurrent (s_overcurrent_blocked, só reset HW) de timeout de heartbeat (restaurável por heartbeat).
- FreeRTOS 200 Hz (tick 5 ms) — obrigatório para MotionSafety P22 @ 5ms.
- FaceTracker: PID pan, dead zone ±5%, cooldown 500ms pós-diálogo, fallback center 5s sem face.

**Sistema**
- OTA: ECDSA P-256, rollback 60s, CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y.
- FactoryReset: triple-tap touch + smoke tests em -DQA_BUILD.
- CloudBridge: Whisper API, T_local 300ms, T_soft 600ms, T_hard 1200ms.

### Pendencias reais (nao sao etapas novas)

- [ ] **Soak 8h** com todas as tasks ativas — criterio de pronto E34 nunca executado
- [ ] **Re-gravar templates KWS** — threshold temporario 120.0 ate templates novos prontos; usar /sdcard/kws/.record ou console serial
- [ ] **PowerManager** — unico stub real restante (boot_sequence.c:76); MAX17048 + bq25185 + deep sleep
- [ ] **I2S full-duplex** mic+amp simultaneo (hal_init.h:62, baixa prioridade — sequencial funciona)

### Criterios de pronto (estado atual)
- [x] Todas as 43 etapas implementadas e comittadas
- [x] Build ok
- [ ] Soak 8h sem crashes
- [ ] Templates KWS re-gravados com nova pipeline (Subsequence DTW, sem C0)

---

## 🧩 Template de Prompt

Cole no inicio de cada nova sessao (preencha os [...]):

```
## CONTEXTO DO PROJETO
Projeto: robo desktop companion, ESP32-S3-WROOM N16R8 (512KB SRAM, 8MB PSRAM, 16MB flash).
Hardware: ST7789 240x320 (LovyanGFX), OV2640, INMP441, MAX98357A, 2x SCS0009 via FE-TTLinker,
          IMU, touch 4 zonas, WS2812x2, LiPo 3Ah.
Arquitetura: Core 0 = WiFi/camera/wake word. Core 1 = face/behavior/audio/sensores/safety.
Regras: offline-first, sem malloc em hot paths, EventBus entre modulos,
        face EMO-style procedural 20fps via LGFX_Sprite.
Etapa atual: [E0N - NOME]. Ultimo concluido: [ITEM]. Proximo: [ITEM].

## ETAPA ATUAL
[Descricao do que fazer, entradas e saidas esperadas]

## INTERFACES EXISTENTES
[Apenas os .h dos modulos que esta etapa usa - nunca os .c]

## RESTRICOES CRITICAS
- Sem malloc em hot paths
- Todo movimento de servo passa por motion_safety_check()
- LovyanGFX: renderizar no LGFX_Sprite *draw_buf, nunca direto no lcd
- EventBus para comunicacao entre modulos, sem chamadas diretas

## SAIDA ESPERADA
[Formato exato: ex. "face_params.h + face_renderer.hpp + face_renderer.h (wrapper C)"]
```

---

## 🏆 Regras de Ouro

| Fazer                                        | Evitar                               |
|----------------------------------------------|--------------------------------------|
| Um arquivo de saida por prompt               | Pedir 5 arquivos de uma vez          |
| Passar so .h dos modulos usados              | Passar .c dos modulos                |
| Especificar formato de saida exato           | "Implemente o sistema" (vago)        |
| Abrir nova conversa por etapa                | Continuar 50+ trocas                 |
| Colar backtrace exato ao reportar bug        | "Nao esta funcionando"               |
| **Executar commit ao concluir cada etapa**   | Deixar trabalho sem commitar         |
