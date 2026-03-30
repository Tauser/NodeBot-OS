# EMO - Especificacao Completa e Adaptacao ao Projeto

Especificacao mestre de comportamento do robo, adaptada para este projeto com foco em timing, silhueta dos olhos e comportamento geral.

## Regra de adaptacao facial

No projeto, o padrao EMO foi consolidado sem camadas faciais internas separadas.

A expressividade facial vem de:

- geometria procedural dos olhos
- abertura vertical (`open_l`, `open_r`)
- squint diagonal
- cor de destaque
- timing de transicoes, blink e idle

## Separacao de responsabilidades

- Face base: shape salvo da expressao.
- Refinamento da expressao: `squint` pode fazer parte da face base quando necessario.
- Runtime: gaze, blink, idle drift, micro-movimentos e tracking modulam a face base sem redefinir o shape.

## Mapa de etapas afetadas

| Sub-spec EMO                | Usar em etapa | Arquivo a criar              |
|----------------------------|---------------|------------------------------|
| Face Expression System     | E17, E18, E19 | face_params.h, face_renderer |
| Animation Library          | E19, E22, E34 | animations.h, animation_system |
| Behavioral State Machine   | E33           | behavior_fsm.h, behavior_tree |
| Idle Behavior Engine       | E20, E22      | idle_behavior_engine.h       |
| Touch & Physical Reaction  | E26           | touch_reaction_service.h     |
| Music & Audio Reaction     | E28, E29      | music_detector.h, dance_sync |
| Voice & Dialogue System    | E32           | intent_expression_mapper.h   |
| Routine & Circadian System | E35           | circadian_service.h          |
| Social & Attention System  | E35           | social_attention_service.h   |
| Power & Battery Behavior   | E35           | power_behavior_service.h     |

---

# 1. Face Expression System

## 1.1 face_params_t

```c
typedef enum {
  FACE_PRIORITY_COSMETIC  = 0,
  FACE_PRIORITY_MOOD      = 1,
  FACE_PRIORITY_IDLE      = 2,
  FACE_PRIORITY_ROUTINE   = 3,
  FACE_PRIORITY_REACTION  = 4,
  FACE_PRIORITY_DIALOGUE  = 5,
  FACE_PRIORITY_SYSTEM    = 6,
  FACE_PRIORITY_SAFETY    = 7,
} face_priority_t;

typedef struct {
  int8_t   tl_l, tr_l, bl_l, br_l;
  int8_t   tl_r, tr_r, bl_r, br_r;
  float    open_l, open_r;
  int8_t   y_l, y_r;
  uint8_t  x_off;
  uint8_t  rt_top, rb_bot;
  int8_t   cv_top, cv_bot;
  uint16_t color;
  float    squint_l, squint_r;
  uint16_t transition_ms;
  face_priority_t priority;
} face_params_t;
```

Observacao:
- `gaze` existe no pipeline do runtime, mas nao precisa ser entendido como parte do shape base da expressao.

## 1.2 Expressoes base

| Expressao  | eyelid L/R | squint L/R | accent   |
|------------|------------|------------|----------|
| NEUTRAL    | 0.80/0.80  | 0.00/0.00  | branco   |
| HAPPY      | 0.50/0.50  | 0.40/0.40  | amarelo  |
| VERY_HAPPY | 0.42/0.42  | 0.55/0.55  | amarelo  |
| LOVE       | 0.55/0.55  | 0.35/0.35  | rosa     |
| SAD        | 0.55/0.55  | 0.20/0.20  | azul     |
| ANGRY      | 0.38/0.38  | 0.70/0.70  | vermelho |
| CURIOUS    | 0.95/0.65  | 0.00/0.15  | branco   |
| THINKING   | 0.72/0.72  | 0.10/0.10  | branco   |
| SURPRISED  | 1.00/1.00  | 0.00/0.00  | branco   |
| TIRED      | 0.22/0.22  | 0.00/0.00  | branco   |
| SLEEPING   | 0.03/0.03  | 0.00/0.00  | branco   |

## 1.3 Prioridades

```text
P7 SAFETY    - SafeMode, brownout: face neutra fixa
P6 SYSTEM    - bateria critica, temperatura, boot
P5 DIALOGUE  - falando ou ouvindo
P4 REACTION  - toque, wake word, queda
P3 ROUTINE   - hora do dia, sono, acordar
P2 MOOD      - estado emocional do StateVector
P1 IDLE      - variacoes organicas de idle
P0 COSMETIC  - micro-movimentos, blink
```

## 1.4 Renderizacao com LovyanGFX

```cpp
void FaceRenderer::drawEye(LGFX_Sprite* s, const EyeParams& p, int cx, int cy) {
  int eh = (int)(56 * p.open);
  if (eh < 2) eh = 2;

  drawEyeShape(s, cx, cy, p, p.color);

  if (p.squint > 0.02f) {
    int rise = (int)(eh * 0.55f * p.squint);
    drawSquintMask(s, cx, cy, rise);
  }
}
```

Observacao:
- gaze e aplicado em runtime por cima da face base.

## 1.5 Blink controller

| Tipo          | Freq      | Descricao                        |
|---------------|-----------|----------------------------------|
| BLINK_NORMAL  | 12-18/min | Ambos, 80ms fechar + 120ms abrir |
| BLINK_SLOW    | 4-8/min   | 150ms + 200ms                    |
| BLINK_FAST    | 25-35/min | 40ms + 60ms                      |
| BLINK_HALF    | ocasional | Fecha so metade                  |
| BLINK_WINK_L  | trigger   | So olho esquerdo                 |
| BLINK_WINK_R  | trigger   | So olho direito                  |
| BLINK_DOUBLE  | trigger   | Pisca 2x rapido                  |
| BLINK_SQUINT  | idle      | Squint sobe por 500ms            |

---

# 2. Animation Library

## Catalogo resumido

```text
ANIM_BOOT:
  accent azul -> NEUTRAL

ANIM_WAKE_UP:
  SLEEPING -> YAWNING -> BLINK_DOUBLE -> NEUTRAL

ANIM_SLEEP_DOWN:
  NEUTRAL -> TIRED -> VERY_TIRED -> SLEEPING

ANIM_HAPPY_REACT:
  NEUTRAL -> SURPRISED -> HAPPY -> VERY_HAPPY -> HAPPY

ANIM_LOVE:
  HAPPY -> LOVE

ANIM_DANCE_LOOP:
  alterna EXCITED e VERY_HAPPY no ritmo
```

---

# 3. Behavioral State Machine

## StateVector

```c
typedef struct {
  float energy;
  float mood_valence;
  float mood_arousal;
  float social_need;
  float attention;
  float affinity;
  float comfort;
  bool music_detected;
  bool person_present;
  bool being_touched;
  bool charging;
} state_vector_t;
```

## Expressao derivada do StateVector

```c
face_params_t derive_expression_from_state(const state_vector_t *sv) {
  face_params_t p = EXPR_NEUTRAL;
  p.open_l = p.open_r = 0.4f + sv->energy * 0.5f;
  p.squint_l = p.squint_r = sv->mood_arousal * 0.15f;
  return p;
}
```

Observacao:
- gaze social, tracking e idle devem ser tratados em runtime, por cima da expressao base.

---

# 4. Idle Behavior Engine

| Comportamento    | Parametros                               |
|------------------|------------------------------------------|
| Micro-gaze drift | gaze_x +/-1.5px @ 0.28Hz                 |
| Breathing gaze   | gaze_y +/-0.03 @ 0.22Hz                  |
| Blink automatico | taxa variavel por energy                 |
| Olhar para lado  | 8-20s                                    |
| Olhar para cima  | 15-35s                                   |
| Squint pensativo | 12-25s                                   |

---

# 5. Touch, Music e Voz

- PET mode: HAPPY -> LOVE -> VERY_HAPPY com servos suaves.
- Dance mode: alterna expressoes positivas conforme a batida.
- Listening/speaking: varia gaze e squint por cima da face base.

---

# 6. Power e bateria

| Nivel    | %      | Expressao        | Comportamento                |
|----------|--------|------------------|------------------------------|
| LOW      | 20-50  | Normal           | Sem mudanca visivel          |
| WARNING  | 10-20  | TIRED gradual    | Menos animacoes              |
| CRITICAL | 5-10   | VERY_TIRED       | Servos off                   |
| DYING    | <5     | SLEEPING forcado | Apenas display               |
| CHARGING | qualq. | accent verde     | Feliz, animacoes mais lentas |

---

# 7. Mapa de arquivos

```text
src/
  models/
    face_params.h          <- E17: struct do shape base
    state_vector.h         <- E21

  face/
    face_renderer.hpp/.cpp <- E17: renderizador procedural
    blink_controller.h/.c  <- E18
    animations.h/.c        <- E19/E22
    animation_system.h/.c  <- E19
    idle_gaze.h/.c         <- E20
```

## Checklist de fidelidade adaptada

- [ ] Squint diagonal implementado
- [ ] Blink com pelo menos 4 tipos
- [ ] Idle tier 1 sempre ativo
- [ ] Idle tier 2 com comportamentos ocasionais
- [ ] Animacoes com keyframes
- [ ] Animacoes usam servos
- [ ] Music detection entra em dance mode
- [ ] Touch por zona com reacoes diferentes
- [ ] PET mode com progressao
- [ ] Wake up animation completa
- [ ] Sleep animation completa
- [ ] StateVector dirige expressao base
- [ ] Battery low muda comportamento visivelmente
- [ ] Runtime modula gaze sem redefinir o shape base
