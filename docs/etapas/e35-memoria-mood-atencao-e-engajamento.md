# E35 - Memória, Mood, Atenção e Engajamento

- Status: ✅ Critérios atendidos
- Complexidade: Alta
- Grupo: Personagem
- HW Real: SIM
- Notas: state_vector_t completo, affinity persistida, circadian integrado. Ver págs. 8, 9 da Análise de Comportamento.
- Prioridade: P3
- Risco: BAIXO
- Depende de: E34

## Memória, Mood, Atenção e Engajamento

Complexidade: Alta
Depende de: E34
Grupo: Personagem
HW Real: SIM
ID: E35
Notas: state_vector_t completo, affinity persistida, circadian integrado. Ver págs. 8, 9 da Análise de Comportamento.
Prioridade: P3
Risco: BAIXO
Status: ✅ Critérios atendidos

## Objetivo

PreferenceMemory, MoodService, AttentionService e EngagementService. O personagem passa a ter profundidade, memória e intenção percebida.

## Parâmetros de decay do MoodService

| Dimensão | Tau | Comportamento |
| --- | --- | --- |
| valence (-1→1) | 4 horas | Muda visivelmente ao longo do dia |
| arousal (0→1) | 30 minutos | Mais reativo a eventos recentes |

## Critérios de pronto

- Boot: nome do usuário na saudação após primeiro setup
- Mood: sessão de 30min com interações vs. sem interações → mood_final diferente
- AttentionService: toque → gaze para zona do toque em < 200ms
- EngagementService: 12 minutos sem interação → comportamento de busca de atenção visível

## Testes mínimos

- Boot: verificar nome na saudação após primeiro setup
- Mood: comparar mood final de duas sessões (com e sem interação)
- Atenção: toque → gaze em < 200ms (medir com log timestamps)
- Engajamento: deixar 12 minutos sem interação

---

## ▶ Prompt Principal

```
Contexto: E33 (BehaviorEngine), E21 (StateVector). E35 — serviços de personagem.
Tarefa A: preference_memory_service.h/.c — user_name, volume, response_style em NVS; últimas 7 sessões em SD.
Tarefa B: mood_service.h/.c — vetor {valence(-1→1), arousal(0→1)}, decay conforme tabela acima. Publicar EVT_MOOD_CHANGED quando muda > 0.05.
Tarefa C: attention_service.h/.c — assinar EVT_TOUCH_DETECTED, EVT_VOICE_ACTIVITY, EVT_MOTION_DETECTED; chamar gaze_service_set_target() para o estímulo de maior prioridade.
Tarefa D: engagement_service.h/.c — rastrear last_interaction_ms; se > 600000ms (10min): publicar EVT_LOW_ENGAGEMENT a cada 30s.
Saída: todos os 4 serviços.
```

## ◎ Prompt de Revisão

```
Serviços de personagem da E35.
Verificar: (1) MoodService escreve NVS apenas quando mudou (não a cada tick)? (2) AttentionService resolve gaze em < 200ms? (3) EngagementService tem cooldown de 30s? (4) decay de valence é lento (horas, não minutos)?
Listar problemas.
```

## ✎ Prompt de Correção

```
MoodService/AttentionService com problema: [sintoma]
Contexto: E35.
Causa + impacto no personagem.
```

## → Prompt de Continuidade

```
E35 concluída. Personagem completo com memória, mood, atenção e engajamento.
Próximas: E36 (CameraService) e E37 (CloudBridge) — podem ser feitas em paralelo.
Próxima: E36 — CameraService como serviço assíncrono no Core 0.
```


