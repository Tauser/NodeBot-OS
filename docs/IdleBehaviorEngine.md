# 4. Idle Behavior Engine

> Sub-página de: 🤖 EMO — Análise Completa
> 

> O EMO nunca fica completamente parado. Idle behavior é o que torna o robô vivo quando não há interacão.
> 

---

## 4.1 Categorias de Idle Behavior

### Idle Tier 1 — Sempre ativo (micro-escala, a cada frame)

| Comportamento | Descrição | Parâmetros |
| --- | --- | --- |
| Micro-gaze drift | Oscilação senoidal do olhar | gaze_x ±1.5px @ 0.28Hz, gaze_y ±1.0px @ 0.19Hz |
| Breathing pupil | Pulsação suave da pupila | pupil_size ±0.03 @ 0.22Hz |
| Blink automático | Taxa variavel por energy | Ver blink_controller |

### Idle Tier 2 — Ocasional (a cada 8-45 segundos)

| Comportamento | Freq | Condição | Duração |
| --- | --- | --- | --- |
| Olhar para lado | rand 8-20s | idle | 1.5s |
| Olhar para cima | rand 15-35s | thinking or idle | 2s |
| Squint pensativo | rand 12-25s | idle | 800ms |
| Piscar devagar | rand 10-20s | energy < 0.5 | 600ms |
| Piscada rápida dupla | rand 20-40s | arousal > 0.5 | 300ms |
| Wink esquerdo | rand 45-90s | mood positivo | 200ms |
| Leve sorriso | rand 20-40s | valence > 0 | 1s |
| Sobrancelha levantada | rand 15-30s | idle | 500ms |

### Idle Tier 3 — Raro (a cada 2-20 minutos)

| Comportamento | Freq | Condição | Animação |
| --- | --- | --- | --- |
| Bocejo | rand 5-15min | energy < 0.5 | ANIM_YAWN |
| Espirro | rand 10-30min | sempre | ANIM_SNEEZE |
| Soluço | rand 8-20min | sempre | ANIM_HICCUP |
| Alongamento | rand 20-40min | sempre | ANIM_STRETCH |
| Olhar ao redor (curioso) | rand 3-8min | social_need > 0.4 | ANIM_LOOK_AROUND |
| Piscada lenta emocionada | rand 5-12min | valence > 0.3 | ANIM_SLOW_BLINK_HAPPY |

---

## 4.2 Implementação do Idle Behavior Engine

```c
// src/behavior/idle_behavior_engine.h

typedef struct {
  uint32_t next_fire_ms;    // quando disparar o proximo
  uint32_t min_interval_ms;
  uint32_t max_interval_ms;
  float    condition_min;   // valor minimo do condicional
  float    condition_max;   // valor maximo
  void     (*action)(void); // funcao a executar
  const char *name;         // para debug/log
} idle_trigger_t;

// Tabela de triggers tier 2
static idle_trigger_t tier2_triggers[] = {
  // {next, min, max, cond_min, cond_max, action, name}
  {0, 8000,  20000, 0.0f, 1.0f, idle_look_side,         "look_side"},
  {0, 15000, 35000, 0.0f, 1.0f, idle_look_up,           "look_up"},
  {0, 12000, 25000, 0.0f, 1.0f, idle_squint_think,      "squint"},
  {0, 10000, 20000, 0.0f, 0.5f, idle_slow_blink,        "slow_blink"},  // so se cansado
  {0, 20000, 40000, 0.5f, 1.0f, idle_double_blink,      "double_blink"}, // so se energico
  {0, 45000, 90000, 0.3f, 1.0f, idle_wink,              "wink"},
  {0, 20000, 40000, 0.0f, 1.0f, idle_slight_smile,      "smile"},
  {0, 15000, 30000, 0.0f, 1.0f, idle_brow_raise,        "brow_raise"},
};

// Tabela de triggers tier 3 (animacoes)
static idle_trigger_t tier3_triggers[] = {
  {0, 300000, 900000,  0.0f, 0.5f, play_anim_yawn,    "yawn"},    // energy < 0.5
  {0, 600000, 1800000, 0.0f, 1.0f, play_anim_sneeze,  "sneeze"},
  {0, 480000, 1200000, 0.0f, 1.0f, play_anim_hiccup,  "hiccup"},
  {0, 1200000,2400000, 0.0f, 1.0f, play_anim_stretch, "stretch"},
  {0, 180000, 480000,  0.4f, 1.0f, play_anim_look_around, "look_around"},
};

void idle_behavior_tick(const emo_state_vector_t *sv, uint32_t now_ms) {
  // Tier 1: sempre (feito no micro_movement_tick, separado)

  // Tier 2: verificar cada trigger
  for (int i = 0; i < ARRAY_SIZE(tier2_triggers); i++) {
    idle_trigger_t *t = &tier2_triggers[i];
    if (now_ms < t->next_fire_ms) continue;
    // Verificar condicional (ex: energy)
    float relevant_val = get_condition_val(i, sv);
    if (relevant_val < t->condition_min || relevant_val > t->condition_max) {
      // Remarcar para daqui a pouco
      t->next_fire_ms = now_ms + t->min_interval_ms;
      continue;
    }
    t->action();
    // Remarcar
    uint32_t interval = t->min_interval_ms +
      (rand() % (t->max_interval_ms - t->min_interval_ms));
    t->next_fire_ms = now_ms + interval;
  }

  // Tier 3: animacoes (so se nenhuma anim ativa)
  if (!anim_is_playing()) {
    for (int i = 0; i < ARRAY_SIZE(tier3_triggers); i++) {
      idle_trigger_t *t = &tier3_triggers[i];
      if (now_ms < t->next_fire_ms) continue;
      float relevant_val = get_condition_val_t3(i, sv);
      if (relevant_val < t->condition_min) {
        t->next_fire_ms = now_ms + t->min_interval_ms;
        continue;
      }
      t->action();
      uint32_t interval = t->min_interval_ms +
        (rand() % (t->max_interval_ms - t->min_interval_ms));
      t->next_fire_ms = now_ms + interval;
      break;  // Uma animacao por tick
    }
  }
}
```

---

## 4.3 Idle Gaze — Sistema de Gaze Automético

```c
// src/behavior/idle_gaze.h
// O gaze do EMO nunca é completamente aleatório.
// Tem uma distribuicao de probabilidade baseada no contexto.

typedef struct {
  float target_x, target_y;  // posicao alvo atual
  float current_x, current_y;
  float speed;               // pixels/s da saccade atual
  uint32_t next_saccade_ms;  // quando fazer a proxima saccade
  bool saccade_in_progress;
} idle_gaze_state_t;

// Distribuicao de probabilidade de posicao idle
// (onde o EMO provavelmente olha em idle)
// Formato: {x, y, peso}
static const float gaze_dist[][3] = {
  { 0.0f,  0.0f, 4.0f },  // centro - mais frequente
  {-0.3f, -0.2f, 1.5f },  // cima-esquerda (pensativo)
  { 0.3f, -0.2f, 1.5f },  // cima-direita
  {-0.4f,  0.1f, 1.0f },  // esquerda
  { 0.4f,  0.1f, 1.0f },  // direita
  { 0.0f, -0.35f,0.8f },  // cima
  { 0.0f,  0.3f, 0.5f },  // baixo (mais raro)
};

// Saccade com overshoot tipico do EMO
void idle_gaze_saccade_to(float tx, float ty) {
  const float OVERSHOOT = 0.12f;
  float ox = gaze_state.current_x + (tx - gaze_state.current_x) * (1 + OVERSHOOT);
  float oy = gaze_state.current_y + (ty - gaze_state.current_y) * (1 + OVERSHOOT);
  // Primeiro vai para overshoot, depois retorna para target
  anim_gaze_to(ox, oy, 60);   // rapido ate overshoot
  // Depois agendar retorno para target em 80ms
}
```

---

## 4.4 Music Detection e Dance Mode

```c
// src/behavior/music_detector.h
// Deteccao de batida musical via VAD e analise de ritmo

typedef struct {
  float   bpm;            // batidas por minuto detectadas
  float   beat_confidence;// 0.0-1.0
  uint32_t last_beat_ms;  // timestamp do ultimo beat
  uint8_t beat_count;     // beats consecutivos detectados
  bool    is_music;       // threshold de confianca atingido
} music_state_t;

// Deteccao de beat por analise de energia de audio
// Algoritmo: peak detection em janelas de 100ms
// BPM tipico: 60-180, abaixo ou acima = nao musica

void music_detector_tick(const int16_t *audio_buf, size_t len);
bool music_is_detected(void);
float music_get_bpm(void);
uint32_t music_next_beat_ms(void);  // estimativa do proximo beat

// Sincronizacao de dance com o beat:
void dance_sync_to_beat(void) {
  uint32_t beat_interval_ms = (uint32_t)(60000.0f / music_get_bpm());
  // Agendar keyframe de servo a cada beat
  servo_schedule_move(PAN_SERVO_ID,
    dance_direction ? 550 : 474,  // alternar L/R
    beat_interval_ms * 0.8f,      // 80% do intervalo para suavidade
    400);                          // speed
  dance_direction = !dance_direction;
}
```