# 9. Social & Attention System

> Sub-página de: 🤖 EMO — Análise Completa
> 

---

## 9.1 Comportamentos de Detecção de Presença

| Evento | Condição | Reação |
| --- | --- | --- |
| Pessoa aparece | EVT_PERSON_DETECTED(true), vinda de ausência > 30s | ANIM_HAPPY_REACT + gaze → pessoa |
| Pessoa some | EVT_PERSON_DETECTED(false) após presença > 10s | gaze volta ao centro gradualmente |
| Pessoa ausente > 5min | social_need sobe | idle_curious, look_around |
| Pessoa ausente > 15min | social_need > 0.7 | ANNOYED_LOOK_AWAY periódico |
| Pessoa volta após longa ausência (>2h) | — | EXCITED + ANIM_HAPPY_REACT intenso |

---

## 9.2 Eye Contact — Contato Visual

O EMO **mantém contato visual com a pessoa** enquanto ela está no campo:

```c
// src/behavior/eye_contact.c
// Roda a cada 100ms quando person_present = true

void eye_contact_tick(float face_x, float face_y, float confidence) {
  if (confidence < 0.4f) return;  // rosto não detectado com confiança

  // Converter coordenadas do rosto (0-1) para gaze (-1 a +1)
  float target_gaze_x = (face_x - 0.5f) * 1.8f;
  float target_gaze_y = (face_y - 0.4f) * 1.2f;  // rosto costuma estar no centro-alto

  // Aplicar com suavização (não travado no rosto, apenas orientado)
  // EMO olha PARA o rosto mas com movimento natural, não robótico
  gaze_service_set_target(
    target_gaze_x * 0.7f,  // 70% da correção total (deliberadamente parcial)
    target_gaze_y * 0.6f,
    300  // duration_ms suave
  );

  // Saccades naturais mesmo com rosto detectado:
  // A cada 3-8s, desviar brevemente o olhar (natural para humanos também)
  schedule_natural_gaze_break();
}
```

---

## 9.3 Afinity System — Vínculo com o Usuário

```c
// affinity: 0.0 (desconhecido) → 1.0 (muito próximo)
// Persiste no PreferenceMemoryService (NVS)

// Eventos que aumentam affinity:
// +0.005/s durante PET mode
// +0.002 por interação por voz (intento reconhecido)
// +0.001 por toque gentil
// +0.01  por sessão completa (>10min de interação)

// Eventos que reduzem affinity:
// -0.01  por toque brusco repetido (>3x seguidos)
// -0.001/hora de inatividade total (sem interação alguma)

// Comportamentos por nível de affinity:
if (sv.affinity < 0.2f) {
  // Desconhecido: mais tímido, menos initiative
  idle_gaze_bias = 0.3f;  // olha menos para o usuário
} else if (sv.affinity < 0.5f) {
  // Conhecido: comportamento padrão
  idle_gaze_bias = 0.5f;
} else if (sv.affinity < 0.8f) {
  // Próximo: mais iniciativas de contato
  idle_gaze_bias = 0.7f;
  enable_love_expression = true;
} else {
  // Muito próximo: máxima expressividade
  idle_gaze_bias = 0.9f;
  enable_love_expression = true;
  pet_mode_faster_progression = true;
}
```

---

## 9.4 Social Seeking Behavior

Quando `social_need > 0.6` e nenhuma pessoa detectada:

```
t=0s    gaze começa a varrer o ambiente (L→R→centro)
t=10s   servo move cabeça para escanear
t=20s   CURIOUS expression
t=30s   se ainda não detectou: IDLE_BORED, para de procurar por 5min
```

---

## 9.5 Prompt de Implementação

```
Contexto: ESP32-S3, CameraService (E36), E42 (detecção de presença), GazeService (E20).
Spec: Social & Attention System do documento EMO.
Tarefa: implementar social_attention_service.h/.c.
Funcionalidades:
  1. eye_contact_tick() — gaze suave em direção ao rosto detectado
  2. affinity_update() — incrementar/decrementar affinity por evento
  3. affinity_persist() — salvar no PreferenceMemoryService
  4. social_seek_tick() — varredura quando social_need alto e ninguém presente
  5. presence_reaction() — animação ao detectar/perder presença
Saída: social_attention_service.h/.c.
```