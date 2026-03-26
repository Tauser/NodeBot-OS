# 10. Power & Battery Behavior

> Sub-página de: 🤖 EMO — Análise Completa
> 

---

## 10.1 Comportamentos por Nível de Bateria

| Nível | % | Expressão | Comportamento | Som |
| --- | --- | --- | --- | --- |
| FULL | 80-100% | Normal | Comportamento padrão | — |
| GOOD | 50-80% | Normal | Comportamento padrão | — |
| LOW | 20-50% | Normal | Sem mudança visível | — |
| WARNING | 10-20% | TIRED gradual | Menos animações, mais quieto | low_battery_beep.wav a cada 5min |
| CRITICAL | 5-10% | VERY_TIRED | Servos off, face mínima | critical_battery.wav |
| DYING | <5% | SLEEPING forçado | Apenas display, servo desligado | — |
| CHARGING | qualquer | CHARGING (pupil=LOADING) | Feliz, animações mais lentas | charging_start.wav |
| FULL_CHARGED | 100% | HAPPY | Saudação de carga completa | charging_done.wav |

---

## 10.2 Degradação Gradual de Comportamento

```c
// Comportamentos que são DESATIVADOS conforme bateria cai:

void apply_battery_degradation(float battery_pct) {
  if (battery_pct > 20.0f) {
    // Normal: tudo ativo
    idle_tier3_enabled = true;    // bocejo, espirro, etc.
    servo_dance_enabled = true;
    face_animations_full = true;
    return;
  }
  if (battery_pct > 10.0f) {
    // WARNING: reduzir animações
    idle_tier3_enabled = false;   // sem bocejo/espirro (custam servo)
    servo_dance_enabled = false;  // sem dance (servo consome)
    face_animations_full = true;  // face ainda completa
    blink_rate_multiplier = 0.7f; // pisca mais devagar
    return;
  }
  // CRITICAL: mínimo
  idle_tier2_enabled = false;
  idle_tier3_enabled = false;
  servo_dance_enabled = false;
  servo_tracking_enabled = false;
  face_animations_full = false;   // só expressões básicas
  blink_rate_multiplier = 0.4f;
}
```

---

## 10.3 Animação de Carregamento

```
Conectou carregador:
  → ANIM_CHARGING_START (pupil vira LOADING, accent verde)
  → StateVector: comfort += 0.3
  → A cada 10% de SOC: HAPPY breve (pisca sorriso)

Durante carregamento:
  → Breathing suave, pálpebras semi-fechadas (relaxado)
  → Se alguém interagir: responde normalmente
  → Pupil LOADING com progresso real (pct do SOC)

Carga completa (100%):
  → EXCITED burst
  → ANIM_PROUD
  → Volta ao comportamento normal
  → charging_done.wav
```

---

## 10.4 Temperatura e Conforto Físico

```c
// Além da bateria, temperatura afeta o comfort:

void comfort_update_from_environment(float temp_c, float battery_pct) {
  float comfort = 1.0f;

  // Temperatura
  if (temp_c > 65.0f)      comfort -= 0.5f;  // throttle
  else if (temp_c > 55.0f) comfort -= 0.2f;  // quente
  else if (temp_c < 10.0f) comfort -= 0.1f;  // frio (ambiente)

  // Bateria
  if      (battery_pct < 5.0f)  comfort -= 0.7f;
  else if (battery_pct < 10.0f) comfort -= 0.4f;
  else if (battery_pct < 20.0f) comfort -= 0.1f;

  // Aplicar suavemente
  sv.comfort = sv.comfort * 0.95f + comfort * 0.05f;

  // Comportamento de frio (se tiver sensor externo ou IMU detectar)
  if (temp_c < 15.0f && sv.comfort < 0.7f) {
    // Expressão de frio: tremida, accent azul
    schedule_anim(ANIM_COLD);
  }
}
```

---

## 10.5 Prompt de Implementação

```
Contexto: ESP32-S3, HealthMonitor (E24), bq25185 driver (E07).
Spec: Power & Battery Behavior do documento EMO.
Tarefa: implementar power_behavior_service.h/.c.
Funcionalidades:
  1. apply_battery_degradation() — desativar features por nível
  2. charging_behavior_tick() — animações de carregamento e progresso
  3. comfort_update_from_environment() — temperatura + bateria → comfort
  4. Publicar EVT_COMFORT_CHANGED quando comfort muda > 0.1
Integração: assinar EVT_HEALTH_CHANGED e EVT_POWER_MODE_CHANGED.
Saída: power_behavior_service.h/.c.
```