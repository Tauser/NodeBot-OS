# E33 - BehaviorEngine Loop + Behavior Tree

- Status: 🔲 Não iniciada
- Complexidade: Muito Alta
- Grupo: Comportamento
- HW Real: SIM
- Notas: FSM com 20 estados, BT com 10 nós de prioridade, state_vector_t com 7 dimensões + affinity. Ver pág. 3 da Análise de Comportamento.
- Prioridade: P2
- Risco: ALTO
- Depende de: E19-E32

## BehaviorEngine Loop + Behavior Tree

Complexidade: Muito Alta
Depende de: E19-E32
Grupo: Comportamento
HW Real: SIM
ID: E33
Notas: FSM com 20 estados, BT com 10 nós de prioridade, state_vector_t com 7 dimensões + affinity. Ver pág. 3 da Análise de Comportamento.
Prioridade: P2
Risco: ALTO
Status: 🔲 Não iniciada

## Objetivo

BehaviorEngine integrando todos os subsistemas em personagem unificado. Behavior tree tomando decisões coerentes. Soak de 4h validando estabilidade.

## Ordem obrigatória do BehaviorLoop (100ms, Core 1, P12)

```
1. motion_safety_feed_heartbeat()   ← SEMPRE PRIMEIRO
2. state_vector_tick(100)           ← decay de 100ms
3. processar eventos do EventBus    ← sem bloqueio
4. behavior_tree_evaluate()         ← < 10ms
5. publicar via EventBus            ← não chamar serviços direto
```

## Behavior tree mínima

```
if safety_mode       → face_safe
elif battery < 10%   → face_tired + sleep mode
elif wake_word < 5s  → engage
elif touch < 2s      → react_touch
else                 → idle_behavior_tick()
```

## Critérios de pronto

- Soak 4h: zero crashes, transições suaves, personagem coerente
- FSM: percorrer todos os 5 estados via debug sem deadlock
- CPU Core 1 ≤ 75% com tudo ativo

## Testes mínimos

- Coerência: toque → arousal → expressão mais intensa → sequência coerente
- Soak 4h: logar estado a cada 10min, verificar ausência de estados incoerentes
- CPU: medir Core 1 durante conversação ativa (pior caso)

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, todos os serviços E19–E32 disponíveis via EventBus. E33 — BehaviorEngine.
Tarefa: behavior_engine.h/.c + behavior_tree.h/.c.
BehaviorLoopTask: Core 1, P12, vTaskDelayUntil 100ms.
A cada tick: ordem conforme tabela acima.
Behavior tree mínima conforme tabela acima.
StateVector: EVT_TOUCH_DETECTED → arousal + 0.3; EVT_WAKE_WORD → attention = 1.0; bateria LOW → energy - 0.2.
FSM de alto nível: SLEEP → IDLE → ENGAGED → TALKING → SAFE_MODE.
Saída: behavior_engine.h/.c + behavior_tree.h/.c.
```

## ◎ Prompt de Revisão

```
BehaviorEngine da E33.
Verificar: (1) BehaviorLoop alimenta heartbeat do MotionSafety a cada tick? (2) sem I/O bloqueante no loop? (3) StateVector clamped em todos os limites? (4) FSM tem transições definidas para todos os estados? (5) CPU Core 1 ≤ 75%?
Listar problemas.
```

## ✎ Prompt de Correção

```
BehaviorEngine com problema: [sintoma — ex: personagem muda de humor abruptamente]
Contexto: E33.
Identificar se é StateVector (decay rápido) ou BT (condição errada). Causa + fix.
```

## → Prompt de Continuidade

```
E33 concluída. BehaviorEngine integrando todos os subsistemas.
Próxima: E34 (GestureService, PersonaService e soak 8h).
Mostre como implementar GestureService com 3 gestos básicos de servo integrados ao MotionSafetyService.
```


