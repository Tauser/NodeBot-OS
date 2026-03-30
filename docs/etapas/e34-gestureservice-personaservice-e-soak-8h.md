# E34 - GestureService, PersonaService e Soak 8h

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Comportamento
- HW Real: SIM
- Notas: ⚠️ SPEC EMO: Gestos usam tabela de servo da pág. 2 (Animation Library). PersonaService usa affinity do Social System (pág. 9). Soak 8h valida todo o EMO FSM.
- Prioridade: P2
- Risco: MÉDIO
- Depende de: E33 + E23

## GestureService, PersonaService e Soak 8h

Complexidade: Média
Depende de: E33 + E23
Grupo: Comportamento
HW Real: SIM
ID: E34
Notas: ⚠️ SPEC EMO: Gestos usam tabela de servo da pág. 2 (Animation Library). PersonaService usa affinity do Social System (pág. 9). Soak 8h valida todo o EMO FSM.
Prioridade: P2
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

GestureService com 3 gestos básicos de servo. PersonaService definindo identidade. Soak de 8h como primeiro critério de produto real.

## Gestos obrigatórios

- GESTURE_GREET — servo0: 0°→30°→0° em 400ms
- GESTURE_ATTENTIVE — servo0: 0°→10° em 300ms, hold 2s, retorno
- GESTURE_REST — todos para 0° em 500ms

## Regra de segurança para gestos

**Cada movimento de servo DEVE passar por motion_safety_check() antes de executar. Se BLOCKED: abortar e logar.**

## Critérios de pronto

- SOAK 8H: zero crashes, heap livre estável, transições suaves ao longo de 8h
- Gestos: motion_safety_check() retorna BLOCKED se servo em risco
- PersonaService: mudar nome via config → saudação usa o novo nome

## Testes mínimos

- 3 gestos: observar 10× cada, verificar suavidade e retorno ao centro
- Safety: motion_safety_check com servo simulado em bloqueio → deve retornar BLOCKED
- Soak 8h: rodar script de interações sintéticas, monitorar via log

---

## ▶ Prompt Principal

```
Contexto: E33 (BehaviorEngine), E23 (MotionSafety), servos (E04). E34 — GestureService.
Tarefa A: gesture_service.h/.c.
gesture_service_perform(gesture_t g): verificar motion_safety_check(servo_id, target_pos) para cada passo; se BLOCKED em qualquer passo: abort + log.
Gestos: GESTURE_GREET (servo0: 0°→30°→0° em 400ms), GESTURE_ATTENTIVE (servo0: 0°→10° em 300ms, hold 2s, retorno), GESTURE_REST (todos para 0° em 500ms).
Tarefa B: persona_service.h/.c.
Ler persona.json do SD com: name, pronouns, response_style (CALM/PLAYFUL/MINIMAL).
persona_service_get_name() → const char*.
Saída: gesture_service.h/.c + persona_service.h/.c + persona.json de exemplo.
```

## ◎ Prompt de Revisão

```
GestureService e PersonaService da E34.
Verificar: (1) cada movimento de servo passa por motion_safety_check()? (2) gesto tem retorno ao centro garantido (mesmo se interrompido)? (3) PersonaService lê de config (não hardcoded)? (4) soak 8h passou sem crash?
Listar problemas.
```

## ✎ Prompt de Correção

```
GestureService com problema: [sintoma]
Contexto: E34.
Causa + risco de safety se bug persistir.
```

## → Prompt de Continuidade

```
E34 concluída. Gestos, PersonaService e soak 8h OK.
Comportamento INTEGRADO.
Próxima: E35 (Memória, Mood, Atenção e Engajamento).
Mostre como combinar PreferenceMemoryService + MoodService + AttentionService.
```


