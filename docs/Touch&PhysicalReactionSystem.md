# 5. Touch & Physical Reaction System

> Sub-página de: 🤖 EMO — Análise Completa
> 

---

## 5.1 Mapa de Zonas de Toque → Reação

| Zona | Local físico | Reação imediata | Reação prolongada (>3s) | Reação brusca |
| --- | --- | --- | --- | --- |
| `ZONE_TOP` | Topo da cabeça | ANIM_TOUCH_HEAD_TOP → HAPPY | ANIM_PET → LOVE | ANIM_TOUCH_ROUGH → ANGRY |
| `ZONE_LEFT` | Lateral esquerda | ANIM_TOUCH_SIDE_L → CURIOUS | ANIM_PET lateral | ANIM_TOUCH_ROUGH |
| `ZONE_RIGHT` | Lateral direita | ANIM_TOUCH_SIDE_R → CURIOUS | ANIM_PET lateral | ANIM_TOUCH_ROUGH |
| `ZONE_BASE` | Base / queixo | ANIM_TOUCH_CHIN → EMBARRASSED | ANIM_SHY | ANIM_TOUCH_ROUGH |

---

## 5.2 Classificação de Tipo de Toque

```c
// src/services/touch/touch_classifier.h

typedef enum {
  TOUCH_TYPE_TAP,        // toque rápido < 300ms
  TOUCH_TYPE_HOLD,       // toque mantido 300ms - 3s
  TOUCH_TYPE_PET,        // toque mantido > 3s → caricias
  TOUCH_TYPE_ROUGH,      // múltiplos toques rápidos < 200ms entre si
  TOUCH_TYPE_DOUBLE_TAP, // dois toques em < 400ms na mesma zona
} touch_type_t;

typedef struct {
  touch_zone_t  zone;
  touch_type_t  type;
  uint32_t      duration_ms;
  uint8_t       tap_count;      // para ROUGH
  float         intensity;      // pressão relativa (0.0-1.0)
} touch_event_t;
```

---

## 5.3 PET Mode — Progressão de Carinho

Quando `ZONE_TOP` + `TOUCH_TYPE_PET`:

```
t=0s    → HAPPY leve
t=1s    → HAPPY pleno, purr_loop.wav inicia
t=3s    → LOVE (heart pupils)
t=5s    → estado_interno: affinity += 0.02/s
t=8s    → VERY_HAPPY com STAR pupils pulsando
t=12s+  → plateau em VERY_HAPPY, purr intensifica
Ao soltar → HAPPY → NEUTRAL gradual (3s)
```

**Impacto no StateVector:**

```c
if (touch_type == TOUCH_TYPE_PET) {
  sv->mood_valence  = fminf(1.0f, sv->mood_valence + 0.01f);  // a cada 100ms
  sv->mood_arousal  = fminf(0.8f, sv->mood_arousal + 0.005f);
  sv->social_need   = fmaxf(0.0f, sv->social_need  - 0.02f);  // satisfeito
  sv->affinity      = fminf(1.0f, sv->affinity     + 0.0005f);// permanente
}
```

---

## 5.4 Reações Físicas IMU

| Evento IMU | Threshold | Animação | Impacto StateVector |
| --- | --- | --- | --- |
| Queda livre | \ | accel\ | < 200mg por >100ms |
| Impacto | \ | accel\ | > 3000mg |
| Shake leve | variância > 100mg² | ANIM_SHAKE_REACT | arousal += 0.2 |
| Shake forte | variância > 300mg² | ANIM_VERY_ANGRY | mood_valence -= 0.3 |
| Tilt > 35° por >2s | ângulo | ANIM_TILT_REACT | confused state |
| Tilt > 60° | ângulo | ANIM_CONFUSED + servo emergency stop | comfort -= 0.1 |

---

## 5.5 Implementação — TouchReactionService

```c
// src/services/touch/touch_reaction_service.c

void touch_reaction_on_event(const touch_event_t *evt) {
  // Selecionar animação por zona + tipo
  switch (evt->zone) {
    case ZONE_TOP:
      if      (evt->type == TOUCH_TYPE_ROUGH)    anim_play(&ANIM_TOUCH_ROUGH);
      else if (evt->type == TOUCH_TYPE_TAP)       anim_play(&ANIM_TOUCH_HEAD_TOP);
      else if (evt->type == TOUCH_TYPE_DOUBLE_TAP)anim_play(&ANIM_EXCITED);
      else if (evt->type == TOUCH_TYPE_PET)       enter_pet_mode(ZONE_TOP);
      break;
    case ZONE_LEFT:  case ZONE_RIGHT:
      if      (evt->type == TOUCH_TYPE_ROUGH)    anim_play(&ANIM_TOUCH_ROUGH);
      else if (evt->type == TOUCH_TYPE_TAP)       anim_play(&ANIM_TOUCH_SIDE);
      else if (evt->type == TOUCH_TYPE_PET)       enter_pet_mode(evt->zone);
      break;
    case ZONE_BASE:
      if      (evt->type == TOUCH_TYPE_ROUGH)    anim_play(&ANIM_TOUCH_ROUGH);
      else if (evt->type == TOUCH_TYPE_TAP)       anim_play(&ANIM_TOUCH_CHIN);
      break;
  }

  // Atualizar StateVector
  if (evt->type != TOUCH_TYPE_ROUGH) {
    sv.mood_valence = fminf(1.0f, sv.mood_valence + 0.08f);
    sv.mood_arousal = fminf(1.0f, sv.mood_arousal + 0.20f);
    sv.social_need  = fmaxf(0.0f, sv.social_need  - 0.15f);
  } else {
    sv.mood_valence = fmaxf(-1.0f, sv.mood_valence - 0.25f);
    sv.mood_arousal = fminf(1.0f,  sv.mood_arousal + 0.30f);
  }
  sv.last_interaction_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
}
```

---

## 5.6 Prompt de Implementação

```
Contexto: ESP32-S3, TouchService (E26) funcionando com EVT_TOUCH_DETECTED.
Spec: Touch & Physical Reaction System do documento EMO.
Tarefa: implementar touch_reaction_service.h/.c.
Classificar toque em: TAP, HOLD, PET, ROUGH, DOUBLE_TAP por zona e duração.
Mapear para animação correta por zona + tipo.
Atualizar emo_state_vector_t conforme tabela de impacto.
PET mode: progressão com affinity++ persistida.
Saída: touch_reaction_service.h/.c + touch_classifier.h/.c.
```