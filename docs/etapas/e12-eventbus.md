# E12 - EventBus

- Status: ✅ Critérios atendidos
- Complexidade: Média
- Grupo: Runtime
- HW Real: SIM
- Prioridade: P1
- Risco: MÉDIO
- Depende de: 

## EventBus

Complexidade: Média
Grupo: Runtime
HW Real: SIM
ID: E12
Prioridade: P1
Risco: MÉDIO
Status: ✅ Critérios atendidos

## Objetivo

EventBus funcional: pub/sub desacoplado, pool de 64 eventos pré-alocados em SRAM, sem malloc em runtime, filas por prioridade.

## Critérios de pronto

- 1000 eventos publicados = 1000 entregues (sem drop)
- Sem malloc: verificar com heap_caps_get_free_size() antes e depois de 10.000 eventos
- Thread-safe: pub de Core 0 + sub em Core 1 por 30s sem race condition

## Testes mínimos

- Pub 1000 eventos, verificar contadores de stats
- Pub de task A + sub de task B simultaneamente por 30s — sem race condition
- Encher o pool (65 eventos sem processar): verificar que evento 65 é descartado e contado

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, FreeRTOS, projeto de robô desktop. Etapa E12 — EventBus.
Tarefa: event_bus.h + event_bus.c.
Pool: 64 slots × 64B, estático em SRAM, sem malloc em runtime.
Filas: 4 filas FreeRTOS (SAFETY P20, SYSTEM P15, BEHAVIOR P10, COSMETIC P5).
Interface:
  esp_err_t event_bus_init(void);
  esp_err_t event_bus_publish(uint16_t type, void *payload, size_t len, uint8_t priority);
  esp_err_t event_bus_subscribe(uint16_t type, void (*callback)(uint16_t, void*));
  void event_bus_get_stats(uint32_t *published, *delivered, *dropped);
Payload copiado para o slot do pool (sem referência ao original).
Saída: implementação completa + teste unitário.
```

## ◎ Prompt de Revisão

```
EventBus da E12.
CRÍTICO verificar: (1) sem malloc em runtime? (2) payload copiado (não ponteiro)? (3) thread-safe entre cores? (4) fila SAFETY tem prioridade real sobre BEHAVIOR? (5) pool cheio descarta e incrementa dropped_count?
Listar problemas com severidade.
```

## ✎ Prompt de Correção

```
EventBus com problema: [sintoma — ex: race condition entre Core 0 e Core 1]
Contexto: E12, FreeRTOS, ESP32-S3.
Diagnosticar. Saída: fix + explicação de por que o bug acontece.
```

## → Prompt de Continuidade

```
E12 concluída. EventBus com pub/sub, pool estático, 1000 eventos sem drop.
Próxima: E13 (ConfigManager e NVS).
Mostre como implementar ConfigManager com schema versionado, CRC de integridade e defaults seguros, usando o NVS do ESP-IDF.
```


