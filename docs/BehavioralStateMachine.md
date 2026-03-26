# 3. Behavioral State Machine

> Sub-página de: 🤖 EMO — Análise Completa
> 

> FSM completo com todos os estados, transições, condições e comportamentos derivados da análise do EMO.
> 

---

## 3.1 Hierarquia de Estados

```
SAFE_MODE           (prioridade maxima - sobrescreve tudo)
  |
  +--> SYSTEM_ALERT (bateria critica, superaquecimento)
         |
         +--> OPERATIONAL
                |
                +--> SLEEPING
                |      |
                |      +--> DEEP_SLEEP
                |      +--> LIGHT_SLEEP
                |
                +--> WAKING_UP
                |
                +--> IDLE
                |      |
                |      +--> IDLE_NEUTRAL     // repouso padrao
                |      +--> IDLE_BORED       // sem interacao > 5min
                |      +--> IDLE_CURIOUS     // novo estimulo percebido
                |      +--> IDLE_MUSIC       // musica detectada
                |
                +--> ENGAGED
                |      |
                |      +--> ENGAGED_PERSON   // pessoa detectada
                |      +--> ENGAGED_TOUCH    // sendo tocado
                |      +--> ENGAGED_VOICE    // ouvindo voz
                |
                +--> TALKING
                |      |
                |      +--> LISTENING        // aguardando input
                |      +--> PROCESSING       // processando comando
                |      +--> SPEAKING         // reproduzindo resposta
                |
                +--> REACTING               // animacao de reacao ativa
```

---

## 3.2 Definição Completa de Estados

```c
// src/behavior/behavior_fsm.h

typedef enum {
  // --- Estados de sistema ---
  STATE_SAFE_MODE        = 0,
  STATE_SYSTEM_ALERT     = 1,
  STATE_BOOT             = 2,

  // --- Estados de sono ---
  STATE_DEEP_SLEEP       = 10,
  STATE_LIGHT_SLEEP      = 11,
  STATE_WAKING_UP        = 12,
  STATE_GOING_TO_SLEEP   = 13,

  // --- Estados idle ---
  STATE_IDLE_NEUTRAL     = 20,
  STATE_IDLE_BORED       = 21,
  STATE_IDLE_CURIOUS     = 22,
  STATE_IDLE_MUSIC       = 23,
  STATE_IDLE_STRETCH     = 24,  // animacao de alongamento

  // --- Estados engaged ---
  STATE_ENGAGED_PERSON   = 30,  // pessoa no campo de visao
  STATE_ENGAGED_TOUCH    = 31,
  STATE_ENGAGED_VOICE    = 32,

  // --- Estados de conversa ---
  STATE_LISTENING        = 40,
  STATE_PROCESSING       = 41,
  STATE_SPEAKING         = 42,

  // --- Reacao (sobreposicao temporaria) ---
  STATE_REACTING         = 50,
} robot_state_t;
```

---

## 3.3 Tabela de Transições Completa

### Transições de Boot

| Estado origem | Evento | Estado destino | Ação |
| --- | --- | --- | --- |
| `BOOT` | boot_complete | `IDLE_NEUTRAL` | ANIM_BOOT_COMPLETE |
| `BOOT` | boot_fail | `SAFE_MODE` | ANIM_GLITCH |

### Transições de Sono

| Estado origem | Evento / Condição | Estado destino | Ação |
| --- | --- | --- | --- |
| `IDLE_NEUTRAL` | sem_interacao > 15min | `GOING_TO_SLEEP` | ANIM_SLEEP_DOWN |
| `IDLE_BORED` | sem_interacao > 8min | `GOING_TO_SLEEP` | ANIM_SLEEP_DOWN |
| `IDLE_*` | hora_noturna AND sem_interacao > 5min | `GOING_TO_SLEEP` | ANIM_SLEEP_DOWN |
| `GOING_TO_SLEEP` | anim_complete | `LIGHT_SLEEP` | — |
| `LIGHT_SLEEP` | sem_interacao > 10min | `DEEP_SLEEP` | display dim |
| `LIGHT_SLEEP` | qualquer_estimulo | `WAKING_UP` | ANIM_WAKE_UP |
| `DEEP_SLEEP` | EVT_WAKE_WORD | `WAKING_UP` | ANIM_WAKE_UP |
| `DEEP_SLEEP` | EVT_TOUCH_DETECTED | `WAKING_UP` | ANIM_WAKE_UP |
| `DEEP_SLEEP` | EVT_PERSON_DETECTED | `LIGHT_SLEEP` | brightness up |
| `WAKING_UP` | anim_complete | `IDLE_NEUTRAL` | — |

### Transições de Idle

| Estado origem | Evento / Condição | Estado destino | Ação |
| --- | --- | --- | --- |
| `IDLE_NEUTRAL` | sem_interacao > 5min | `IDLE_BORED` | ANIM_ANNOYED_LOOK_AWAY |
| `IDLE_NEUTRAL` | music_detected | `IDLE_MUSIC` | ANIM_MUSIC_DETECT |
| `IDLE_NEUTRAL` | person_appeared | `ENGAGED_PERSON` | gaze -> pessoa |
| `IDLE_NEUTRAL` | EVT_WAKE_WORD | `LISTENING` | ANIM_WAKE_REACT |
| `IDLE_NEUTRAL` | EVT_TOUCH_DETECTED | `ENGAGED_TOUCH` | anim por zona |
| `IDLE_NEUTRAL` | timer_stretch (rand 20-40min) | `IDLE_STRETCH` | ANIM_STRETCH |
| `IDLE_BORED` | qualquer_interacao | `IDLE_NEUTRAL` | ANIM_HAPPY_REACT |
| `IDLE_BORED` | music_detected | `IDLE_MUSIC` | ANIM_MUSIC_DETECT |
| `IDLE_BORED` | social_need > 0.8 | `IDLE_CURIOUS` | look_for_person |
| `IDLE_CURIOUS` | person_found | `ENGAGED_PERSON` | — |
| `IDLE_CURIOUS` | timeout 30s | `IDLE_NEUTRAL` | — |
| `IDLE_MUSIC` | music_stop | `IDLE_NEUTRAL` ou `IDLE_BORED` | ANIM_MUSIC_STOP |
| `IDLE_MUSIC` | EVT_WAKE_WORD | `LISTENING` | parar dance |
| `IDLE_STRETCH` | anim_complete | `IDLE_NEUTRAL` | — |

### Transições de Engaged

| Estado origem | Evento | Estado destino | Ação |
| --- | --- | --- | --- |
| `ENGAGED_PERSON` | EVT_WAKE_WORD | `LISTENING` | ANIM_WAKE_REACT |
| `ENGAGED_PERSON` | person_gone > 5s | `IDLE_NEUTRAL` | — |
| `ENGAGED_PERSON` | EVT_TOUCH_DETECTED | `ENGAGED_TOUCH` | anim por zona |
| `ENGAGED_TOUCH` | touch_released > 2s | `IDLE_NEUTRAL` | — |
| `ENGAGED_TOUCH` | touch_continued > 3s | `ENGAGED_TOUCH` | ANIM_PET loop |
| `ENGAGED_VOICE` | voice_end | `IDLE_NEUTRAL` | — |
| `ENGAGED_VOICE` | EVT_INTENT_DETECTED | `PROCESSING` | — |

### Transições de Diálogo

| Estado origem | Evento | Estado destino | Ação |
| --- | --- | --- | --- |
| `LISTENING` | timeout 3s | `IDLE_NEUTRAL` | ANIM_CONFUSED |
| `LISTENING` | EVT_INTENT_DETECTED | `PROCESSING` | — |
| `PROCESSING` | timeout 5s | `IDLE_NEUTRAL` | ANIM_CONFUSED |
| `PROCESSING` | resposta_pronta | `SPEAKING` | ANIM_DIALOGUE_EXPR |
| `SPEAKING` | playback_done | `IDLE_NEUTRAL` ou `ENGAGED_PERSON` | — |

### Transições de Sistema

| Estado origem | Evento | Estado destino | Ação |
| --- | --- | --- | --- |
| qualquer | battery < 10% | `SYSTEM_ALERT` | ANIM_BATTERY_CRITICAL |
| qualquer | temp > 75°C | `SYSTEM_ALERT` | display warning |
| qualquer | 3x boot_fail | `SAFE_MODE` | ANIM_GLITCH |
| qualquer | servo_blocked | `REACTING` (temporario) | ANIM_SHOCKED |
| qualquer | fall_detected | `REACTING` (temporario) | ANIM_FALL_DETECTED |

---

## 3.4 StateVector EMO — Vetor de Estado Interno

```c
// src/behavior/state_vector.h

typedef struct {
  // --- Energia e Humor ---
  float energy;        // 0.0-1.0, tau=4h, afeta eyelid e blink rate
  float mood_valence;  // -1.0 a +1.0, tau=6h, afeta brow e smile
  float mood_arousal;  // 0.0-1.0, tau=30min, afeta pupil size e speed

  // --- Social ---
  float social_need;   // 0.0-1.0, sobe quando isolado, cai com interacao
  float attention;     // 0.0-1.0, tau=10min, quanto esta focado
  float affinity;      // 0.0-1.0, tau=permanente, afeto acumulado pelo usuario

  // --- Conforto ---
  float comfort;       // 0.0-1.0, afetado por temperatura, bateria, movimentacao

  // --- Contadores de interacao ---
  uint32_t total_interactions;   // lifetime
  uint32_t session_interactions; // sessao atual
  uint32_t last_interaction_ms;  // timestamp
  uint32_t time_awake_ms;        // acordado desde quando

  // --- Flags de contexto ---
  bool music_detected;
  bool person_present;
  bool being_touched;
  bool charging;
} state_vector_t;
```

---

## 3.5 Behavior Tree do EMO — Loop 10Hz

```c
// Executada a cada 100ms no BehaviorLoop
// Prioridade decrescente:

bt_result_t behavior_tree_tick(state_vector_t *sv, robot_state_t state) {

  // P1: SAFETY
  if (state == STATE_SAFE_MODE)
    return face_apply(EXPR_NEUTRAL_FIXED);

  // P2: SYSTEM ALERT
  if (sv->comfort < 0.1f)  // bateria critica ou temperatura
    return anim_play(ANIM_BATTERY_CRITICAL);

  // P3: EM SONO (manter sono, so acordar com estimulo forte)
  if (state == STATE_DEEP_SLEEP || state == STATE_LIGHT_SLEEP)
    return run_sleep_behavior(sv);

  // P4: ANIMACAO EM ANDAMENTO (respeitar a animacao)
  if (anim_is_playing() && !current_anim.interruptible)
    return BT_RUNNING;

  // P5: DIALOGO ATIVO
  if (state == STATE_LISTENING)  return face_apply(EXPR_LISTENING);
  if (state == STATE_PROCESSING) return face_apply(EXPR_THINKING);
  if (state == STATE_SPEAKING)   return run_speaking_expression(sv);

  // P6: REACAO FISICA RECENTE (< 3s)
  if (last_physical_event_ms < 3000)
    return run_reaction_expression(sv, last_physical_event_type);

  // P7: MÚSICA DETECTADA
  if (sv->music_detected)
    return run_music_behavior(sv);

  // P8: ENGAGED com pessoa
  if (sv->person_present && sv->attention > 0.3f)
    return run_engaged_person_behavior(sv);

  // P9: IDLE BORED
  if (sv->social_need > 0.8f && sv->last_interaction_ms > 300000) {
    // 5+ minutos sem interacao
    if (!anim_is_playing())
      schedule_random_idle_anim(sv);  // ANIM_LOOK_AROUND, ANIM_HICCUP, etc.
  }

  // P10: IDLE NORMAL - derivar da do estado interno
  return run_idle_expression_from_state(sv);
}
```

---

## 3.6 Expressão Derivada do Estado Interno

```c
// Converte StateVector -> face_params_t automaticamente
// Roda quando nenhuma animacao especifica esta ativa

face_params_t derive_expression_from_state(const state_vector_t *sv) {
  face_params_t p = EXPR_NEUTRAL;  // base

  // Energy -> eyelid + blink rate
  float el = 0.4f + sv->energy * 0.5f;  // 0.4 (cansado) a 0.9 (alerta)
  p.eyelid_l = p.eyelid_r = el;
  blink_set_rate(sv->energy);

  // Arousal -> pupil size
  p.pupil_size = 0.3f + sv->mood_arousal * 0.5f;

  // Valence -> brow + smile
  if (sv->mood_valence > 0.2f) {
    // Positivo: brow para cima, leve sorriso
    float v = (sv->mood_valence - 0.2f) / 0.8f;  // 0-1 normalizado
    p.brow_angle_l = -v * 0.35f;
    p.brow_angle_r =  v * 0.35f;
    p.mouth_smile  =  v * 0.5f;
  } else if (sv->mood_valence < -0.2f) {
    // Negativo: brow furrow, leve tristeza
    float v = (-sv->mood_valence - 0.2f) / 0.8f;
    p.brow_angle_l =  v * 0.30f;   // furrow
    p.brow_angle_r = -v * 0.30f;
    p.mouth_smile  = -v * 0.35f;
    p.pupil_shape_l = p.pupil_shape_r = PUPIL_OVAL_DOWN;
  }

  // Social need alto -> gaze procura usuario
  if (sv->social_need > 0.7f)
    p.gaze_x = seek_user_direction();  // 0.0 se nao detectado

  // Attention -> squint leve (foco)
  if (sv->attention > 0.6f) {
    p.squint_l = p.squint_r = (sv->attention - 0.6f) * 0.2f;
  }

  return p;
}
```