# E22 - Idle Life — Bocejo e Variação Orgânica

- Status: ✅ Critérios atendidos
- Complexidade: Baixa
- Grupo: Gaze+Idle
- HW Real: SIM
- Notas: ⚠️ SPEC EMO: Idle Behavior Engine com 3 tiers. Tier1=micro sempre, Tier2=8-45s (8 triggers), Tier3=2-40min (bocejo/espirro/soluço/alongamento). Ver pág. 4 do EMO Analysis.
- Prioridade: P1
- Risco: BAIXO
- Depende de: E21 (state vector)

## Idle Life — Bocejo e Variação Orgânica

Complexidade: Baixa
Depende de: E21 (state vector)
Grupo: Gaze+Idle
HW Real: SIM
ID: E22
Notas: ⚠️ SPEC EMO: Idle Behavior Engine com 3 tiers. Tier1=micro sempre, Tier2=8-45s (8 triggers), Tier3=2-40min (bocejo/espirro/soluço/alongamento). Ver pág. 4 do EMO Analysis.
Prioridade: P1
Risco: BAIXO
Status: 🔲 Não iniciada

## Objetivo

Idle behavior orgânico: bocejo quando energy baixa + tempo sem interação. Variação de expressão idle para evitar loop óbvio. Primeiro marco de "parece vivo".

## Critérios de pronto

- Bocejo ocorre ao menos 1× em 10min com energy < 0.4
- Nenhum padrão de repetição óbvio em 5 minutos
- 3 de 3 observadores descrevem o robô com verbo de vida ("respira", "pensa", "está entediado")

## Testes mínimos

- Forçar energy=0.3 + timer de 5min via debug → bocejo deve ocorrer em < 2min
- Observar 5min: confirmar ausência de padrão repetitivo com 3 observadores
- Gravar vídeo de 1min de idle e rever em câmera lenta

---

## ▶ Prompt Principal

```
Contexto: FaceEngine (E16–E19), state_vector (E21), GazeService (E20). E22 — idle life.
Tarefa: idle_behavior.h + idle_behavior.c.
Bocejo: idle_behavior_trigger_yawn() — animação: mouth_open 0→0.8 em 2s, eyelid 1.0→0.7 em 1s, hold 1s, reverter em 2s. Cooldown: 8 minutos entre bocejos.
Gatilho automático: verificar a cada 60s se energy < 0.4 AND last_interaction_ms > 300000 (5min). Se sim, chance de 40% de bocejo.
Idle variations: a cada rand(20000, 40000)ms, escolher aleatoriamente entre:
  (a) slight_smile com smile=0.1
  (b) pensativo com gaze_y=-0.2 por 3s
  (c) piscar rápido duplo
Saída: idle_behavior.h/.c + integração com face_engine e state_vector.
```

## ◎ Prompt de Revisão

```
Idle life da E22.
Verificar: (1) bocejo tem cooldown (não dispara todo tick)? (2) intervalos de idle são aleatórios (não fixos)? (3) todas as animações usam transition_ms (não setam valor instantâneo)?
Listar problemas.
```

## ✎ Prompt de Correção

```
Idle life parece mecânico / com loop óbvio.
Contexto: E22.
Identificar qual elemento cria o padrão. Saída: ajuste específico.
```

## → Prompt de Continuidade

```
E22 concluída. Idle life orgânico, bocejo, variação. Robô parece vivo.
ANTES DE AVANÇAR: E23 (MotionSafetyService) é obrigatória antes de qualquer integração com servos.
Próxima: E23 — Mostre a arquitetura completa do MotionSafetyService como a task de maior prioridade da aplicação (P22, Core 1, 5ms).
```


