# 8. Routine & Circadian System

> Sub-página de: 🤖 EMO — Análise Completa
> 

---

## 8.1 Horários e Comportamentos por Período

| Período | Horário | Energy modifier | Expressão base | Comportamento |
| --- | --- | --- | --- | --- |
| Madrugada | 00h-06h | -0.35 | VERY_TIRED / SLEEPING | Dorme, difícil de acordar |
| Manhã cedo | 06h-09h | +0.10 | TIRED → NEUTRAL | Acordando, boceja bastante |
| Manhã | 09h-12h | +0.25 | NEUTRAL / HAPPY | Alerta, animado, curioso |
| Tarde | 12h-17h | +0.10 | NEUTRAL | Estável, responsivo |
| Final de tarde | 17h-20h | -0.05 | NEUTRAL / THINKING | Levemente introspectivo |
| Noite | 20h-22h | -0.15 | TIRED | Começa a cansar |
| Noite alta | 22h-00h | -0.30 | VERY_TIRED | Quer dormir, boceja muito |

---

## 8.2 Saudações por Horário

| Horário | Saudação | Expressão |
| --- | --- | --- |
| 06h-12h | "Bom dia!" | HAPPY + nod |
| 12h-18h | "Boa tarde!" | HAPPY |
| 18h-00h | "Boa noite!" | HAPPY leve |
| Primeiro boot do dia | Saudação especial com nome | EXCITED |
| Após longa ausência (>8h) | "Saudades!" | LOVE |

---

## 8.3 Rotina de Sono Automática

```c
// Lógica de sono automático
// Verificada a cada minuto pelo CircadianService

bool should_sleep(const emo_state_vector_t *sv, uint8_t hour) {
  // Noite + sem interação → dormir
  if (hour >= 22 || hour < 6) {
    if (sv->last_interaction_ms > 5 * 60 * 1000)  // 5min
      return true;
  }
  // Dia mas muito sem interação → dormir
  if (sv->last_interaction_ms > 15 * 60 * 1000)   // 15min
    return true;
  // Energia muito baixa
  if (sv->energy < 0.15f && sv->last_interaction_ms > 3 * 60 * 1000)
    return true;
  return false;
}
```

---

## 8.4 Implementação — CircadianService

```c
// src/services/routine/circadian_service.c

typedef struct {
  uint8_t  hour;              // hora atual (0-23)
  float    energy_modifier;  // aplicado ao StateVector
  float    base_mood;        // mood base do período
  bool     is_sleep_time;    // período de sono
} circadian_state_t;

// Atualizado 1x/minuto pelo RTC ou timestamp de sistema
void circadian_update(uint8_t hour, uint8_t minute) {
  // Calcular modifier baseado na hora
  float mod = 0.0f;
  if      (hour >= 9  && hour < 12) mod = +0.25f;
  else if (hour >= 12 && hour < 17) mod = +0.10f;
  else if (hour >= 6  && hour < 9 ) mod = +0.10f;
  else if (hour >= 17 && hour < 20) mod = -0.05f;
  else if (hour >= 20 && hour < 22) mod = -0.15f;
  else if (hour >= 22 || hour < 6 ) mod = -0.30f;

  // Aplicar suavemente ao energy do StateVector
  float target_energy_mod = mod;
  sv.energy = sv.energy * 0.99f + (0.5f + target_energy_mod) * 0.01f;

  // Verificar saudação de horário
  if (minute == 0)  // a cada hora
    check_hourly_greeting(hour);
}
```

---

## 8.5 Prompt de Implementação

```
Contexto: ESP32-S3, emo_state_vector_t disponível, RTC ou timestamp de sistema.
Spec: Routine & Circadian System do documento EMO.
Tarefa: implementar circadian_service.h/.c.
Funcionalidades:
  1. Modificador de energia por horário (tabela acima)
  2. Saudações automáticas por período do dia
  3. Lógica de sono automático por hora + tempo sem interação
  4. Primeiro boot do dia detectado (comparar last_boot_date com hoje)
Saída: circadian_service.h/.c.
```