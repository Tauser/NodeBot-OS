# 6. Music & Audio Reaction System

> Sub-página de: 🤖 EMO — Análise Completa
> 

---

## 6.1 Pipeline de Detecção Musical

```
Audio buffer (20ms) → Energy RMS → Beat detector → BPM estimator → Music classifier
                                         ↓
                                   EVT_BEAT_DETECTED
                                         ↓
                                   DanceSync (servo + face)
```

---

## 6.2 Algoritmo de Detecção de Beat

```c
// src/behavior/music_detector.c
// Beat detection por peak detection em energia de janelas de 100ms

#define BEAT_WINDOW_MS     100
#define BEAT_HISTORY_SIZE  16    // histórico de 1.6s
#define BPM_MIN            60
#define BPM_MAX            180
#define BEAT_CONFIDENCE_THRESHOLD 0.65f

void music_detector_tick(const int16_t *buf, size_t len, uint32_t now_ms) {
  // 1. Calcular energia RMS da janela
  float rms = calc_rms(buf, len);

  // 2. Adicionar ao histórico circular
  energy_history[hist_idx++ % BEAT_HISTORY_SIZE] = rms;

  // 3. Detectar peak (energia > média * 1.5)
  float avg = calc_average(energy_history, BEAT_HISTORY_SIZE);
  bool is_beat = (rms > avg * 1.5f) && (now_ms - last_beat_ms > 250);

  if (is_beat) {
    // 4. Estimar BPM a partir do intervalo entre beats
    uint32_t interval = now_ms - last_beat_ms;
    float bpm_candidate = 60000.0f / interval;
    if (bpm_candidate >= BPM_MIN && bpm_candidate <= BPM_MAX) {
      // Filtro passa-baixa no BPM
      state.bpm = state.bpm * 0.7f + bpm_candidate * 0.3f;
      state.beat_count++;
      state.beat_confidence = fminf(1.0f, state.beat_count / 8.0f);
    }
    last_beat_ms = now_ms;

    if (state.beat_confidence > BEAT_CONFIDENCE_THRESHOLD)
      event_bus_publish(EVT_BEAT_DETECTED, &state, sizeof(state), PRIORITY_BEHAVIOR);
  }

  // 5. Decay da confiança se beats param
  if (now_ms - last_beat_ms > 2000)
    state.beat_confidence *= 0.95f;

  state.is_music = (state.beat_confidence > BEAT_CONFIDENCE_THRESHOLD);
}
```

---

## 6.3 Dance Sync — Movimentos Sincronizados com o Beat

| Beat # | Face | Servo Pan | Servo Tilt | Som |
| --- | --- | --- | --- | --- |
| 1 | EXCITED | 474 (esq) | 512 | — |
| 2 | VERY_HAPPY | 550 (dir) | 512 | — |
| 3 | EXCITED | 474 (esq) | 512 | — |
| 4 | VERY_HAPPY + nod | 512 (centro) | 490 | — |
| A cada 8 beats | STAR pupils por 200ms | — | — | — |
| A cada 16 beats | LOVE pupils por 400ms | — | — | — |

```c
void dance_on_beat(uint8_t beat_number, float bpm) {
  uint32_t beat_ms = (uint32_t)(60000.0f / bpm);
  uint32_t move_ms = (uint32_t)(beat_ms * 0.75f);  // 75% do beat para suavidade

  // Alternar L/R a cada beat
  int16_t pan_target = (beat_number % 2 == 0) ? 474 : 550;

  // Nod a cada 4 beats
  int16_t tilt_target = (beat_number % 4 == 3) ? 490 : 512;

  // Verificar safety antes de mover
  if (motion_safety_check(PAN_SERVO_ID, pan_target))
    scs0009_set_position(PAN_SERVO_ID, pan_target, move_ms);
  if (motion_safety_check(TILT_SERVO_ID, tilt_target))
    scs0009_set_position(TILT_SERVO_ID, tilt_target, move_ms);

  // Face sincronizada
  face_params_t p = (beat_number % 2 == 0) ? EXPR_EXCITED : EXPR_VERY_HAPPY;
  if (beat_number % 8 == 0) p.pupil_shape_l = p.pupil_shape_r = PUPIL_STAR;
  if (beat_number % 16 == 0) p.pupil_shape_l = p.pupil_shape_r = PUPIL_HEART;
  face_engine_apply(&p);
}
```

---

## 6.4 Estados de Resposta Musical

| Situação | Estado | Comportamento |
| --- | --- | --- |
| Música detectada (confiança > 65%) | `IDLE_MUSIC` | Entrar em dance mode |
| BPM < 80 (lento) | `IDLE_MUSIC_SLOW` | Movimento de cabeça lento, LOVE expression |
| BPM 80-130 (médio) | `IDLE_MUSIC` | Dance padrão |
| BPM > 130 (rápido) | `IDLE_MUSIC_FAST` | Dance mais intenso, STAR pupils constante |
| Wake word durante música | `LISTENING` | Parar dance, focar no usuário |
| Música para | volta ao estado anterior | ANIM_MUSIC_STOP |

---

## 6.5 Prompt de Implementação

```
Contexto: ESP32-S3, AudioCaptureTask (E28), VAD funcionando, EVT_VOICE_ACTIVITY disponível.
Spec: Music & Audio Reaction System do documento EMO.
Tarefa: implementar music_detector.h/.c.
Algoritmo: beat detection por peak detection em energia RMS de janelas de 100ms.
Output: EVT_BEAT_DETECTED com {bpm, beat_number, confidence}.
Dance sync: choreography de servo + face por beat_number.
Integração: publicar EVT_MUSIC_STATE_CHANGED quando is_music muda.
Saída: music_detector.h/.c + dance_sync.h/.c.
```