# E23 - MotionSafetyService

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Safety
- HW Real: SIM
- Notas: IMPLEMENTAR ANTES DE QUALQUER MOVIMENTO DE SERVO
- Prioridade: P1
- Risco: CRÍTICO
- Depende de: 

## MotionSafetyService

Complexidade: Alta
Grupo: Safety
HW Real: SIM
ID: E23
Notas: IMPLEMENTAR ANTES DE QUALQUER MOVIMENTO DE SERVO
Prioridade: P1
Risco: CRÍTICO
Status: 🔲 Não iniciada

## ⚠️ ETAPA CRÍTICA — Task de maior prioridade da aplicação

## Objetivo

MotionSafetyService detectando servo bloqueado em < 100ms e parando imediatamente. Heartbeat obrigatório do BehaviorEngine. Fail-safe = estado parado.

## Checklist obrigatório antes de considerar concluída

- [ ]  Task em P22 (maior prioridade da app, acima de tudo exceto WiFi stack)
- [ ]  Sem delay() bloqueante em nenhum caminho
- [ ]  Sem malloc()
- [ ]  Sem I/O de SD nesta task
- [ ]  Heartbeat verificado a cada tick (não a cada 1s)
- [ ]  Fail-safe é "parado" (não "manter última posição")
- [ ]  Limite de corrente com debounce (80ms acumulado, não 1 ciclo)

## Critérios de pronto

- Servo bloqueado mecanicamente: detectado em < 100ms, parado, sem aquecimento após 60s
- Heartbeat: parar de enviar por 600ms → servos param
- P22 verificado: sob carga máxima de Core 1, task roda a cada 5ms

## Testes mínimos — TODOS OBRIGATÓRIOS

- **CRÍTICO:** bloquear servo mecanicamente por 3s → detectado em < 100ms, parado
- Heartbeat: não enviar por 600ms → verificar parada de servos
- Prioridade: adicionar task de alta carga no Core 1 → MotionSafetyTask ainda roda a cada 5ms

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, SCS0009 com get_current_ma(), FreeRTOS Core 1. E23 — MotionSafetyService.
Tarefa: motion_safety_service.h + motion_safety_service.c.
MotionSafetyTask: Core 1, P22, vTaskDelayUntil 5ms.
A cada tick:
  (1) ler get_current_ma(0) e get_current_ma(1)
  (2) se >800mA por >80ms acumulados: scs0009_set_torque_enable(false) + publicar EVT_SERVO_BLOCKED
  (3) verificar heartbeat: se EVT_SERVICE_HEARTBEAT não recebido em 500ms: parar todos os servos
Interface: motion_safety_init(), motion_safety_feed_heartbeat(), motion_safety_emergency_stop(), motion_safety_is_safe() → bool.
Restrição: sem malloc, sem delay(), sem I/O de SD, sem I2C nesta task.
Saída: implementação completa.
```

## ◎ Prompt de Revisão

```
MotionSafetyService da E23.
CRÍTICO verificar: (1) task em P22? (2) sem delay() bloqueante? (3) heartbeat verificado a cada tick? (4) fail-safe é "parado" (não manter posição)? (5) sem malloc?
Listar com severidade CRÍTICO/ALTO.
```

## ✎ Prompt de Correção

```
MotionSafety com falha: [sintoma — ex: servo não para quando corrente chega a 1A]
Contexto: E23.
Diagnosticar com análise de por que o bug é perigoso em produto.
Saída: fix + risco real do bug original.
```

## → Prompt de Continuidade

```
E23 concluída. MotionSafety detecta bloqueio em <100ms, heartbeat funcionando.
Próxima: E24 (HealthMonitor e modos degradados).
Mostre como implementar HealthMonitor que monitora bateria, temperatura e contadores de falha, e como definir os modos NORMAL/LOW_POWER/CRITICAL.
```


