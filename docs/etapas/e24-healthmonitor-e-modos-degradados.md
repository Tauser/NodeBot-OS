# E24 - HealthMonitor e Modos Degradados

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Safety
- HW Real: SIM
- Prioridade: P1
- Risco: ALTO
- Depende de: 

## HealthMonitor e Modos Degradados

Complexidade: Média
Grupo: Safety
HW Real: SIM
ID: E24
Prioridade: P1
Risco: ALTO
Status: 🔲 Não iniciada

## Objetivo

HealthMonitor monitorando bateria, temperatura e falhas de serviço. Transições automáticas entre modos NORMAL → LOW_POWER → CRITICAL_POWER → SHUTDOWN.

## Thresholds

| Métrica | Tier | Valor |
| --- | --- | --- |
| Bateria | LOW | < 20% |
| Bateria | CRITICAL | < 10% |
| Temperatura | WARN | > 55°C |
| Temperatura | THROTTLE | > 65°C |
| Temperatura | SHUTDOWN | > 75°C |
| Heap | WARN | < 50 KB |

## Critérios de pronto

- Bateria simulada a 8%: transição para CRITICAL_POWER, WiFi off, face mínima, servos parados
- Temperatura simulada a 70°C: throttle de CPU para 160MHz
- Heap < 40KB: EVT_HEALTH_CHANGED publicado com severity WARN

## Testes mínimos

- Descarregar bateria abaixo de 20% (ou simular via debug): verificar transição automática para LOW_POWER
- Simular temperatura alta via debug: verificar throttle
- Forçar CRITICAL_POWER: verificar que display dim e WiFi off ocorrem

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, max17048 driver (E07), EventBus (E12). E24 — HealthMonitor.
Tarefa: health_monitor_service.h + health_monitor_service.c.
HealthMonitorTask: Core 1, P6, 1Hz.
A cada tick: (1) ler SOC% e tensão; (2) ler temperatura interna; (3) ler heap livre; (4) se threshold cruzado: publicar EVT_HEALTH_CHANGED com {metric, level, value}.
PowerManager: power_manager_update_mode() chamado em cada EVT_HEALTH_CHANGED.
Modos: NORMAL → LOW_POWER → CRITICAL_POWER → SHUTDOWN.
Comportamento por modo: LOW_POWER → servos off, display dim; CRITICAL → face mínima, WiFi off; SHUTDOWN → salvar estado, parar tudo.
Saída: health_monitor_service.h/.c + power_manager atualizado.
```

## ◎ Prompt de Revisão

```
HealthMonitor da E24.
Verificar: (1) HealthMonitorTask em P6 (não competir com Safety/Face)? (2) publicar EVT_HEALTH_CHANGED só quando tier muda (não a cada tick)? (3) modo SHUTDOWN chama log_flush_now() antes de desligar?
Listar problemas.
```

## ✎ Prompt de Correção

```
HealthMonitor com problema: [sintoma]
Contexto: E24.
Diagnosticar. Causa + fix.
```

## → Prompt de Continuidade

```
E24 concluída. HealthMonitor e modos degradados funcionando.
Próxima: E25 (Brownout handler, SafeMode e LEDs de estado).
Mostre como implementar o brownout ISR do ESP32-S3 que salva estado crítico em NVS antes do reset, e como ativar SafeMode após 3 boots com falha.
```


