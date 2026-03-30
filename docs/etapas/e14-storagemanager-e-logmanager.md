# E14 - StorageManager e LogManager

- Status: ✅ Critérios atendidos
- Complexidade: Média
- Grupo: Runtime
- HW Real: SIM
- Prioridade: P1
- Risco: MÉDIO
- Depende de: 

## StorageManager e LogManager

Complexidade: Média
Grupo: Runtime
HW Real: SIM
ID: E14
Prioridade: P1
Risco: MÉDIO
Status: ✅ Critérios atendidos

## Objetivo

StorageManager abstraindo SD e NVS. LogManager com buffer circular 16KB em PSRAM e flush periódico para SD — log_write() retorna em < 100µs, nunca bloqueia.

## Critérios de pronto

- log_write() retorna em < 100µs em 99% dos casos
- Buffer cheio: descarta entrada mais antiga, não bloqueia
- SD ausente: log_write() não crasha

## Testes mínimos

- 10.000 chamadas a log_write(): verificar que retornam em < 100µs todas
- Remover SD: verificar que log_write() não crasha
- Encher o buffer: verificar que overflow descarta antigo (não bloqueia)

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, SD disponível (E10), PSRAM para buffer. E14 — LogManager.
Tarefa: log_manager.h + log_manager.c.
Interface: log_init(), log_write(level, service, message) — NÃO BLOQUEANTE (<100µs), log_flush_now() para chamar antes de reset.
Buffer: 16KB circular em PSRAM. Flush: StorageFlushTask existente ou timer de 10s.
Formato: JSON por linha — {"l":"[FATAL/ERROR/WARN/INFO/DEBUG]","s":"[service]","m":"[message]","t":[ms_since_boot]}.
Rotação: se log_0.jsonl > 1MB, renomear para log_1.jsonl (descartando log_2.jsonl se existir).
Restrição: log_write() nunca faz I/O de SD — apenas copia para buffer.
Saída: log_manager.h + log_manager.c + teste de latência.
```

## ◎ Prompt de Revisão

```
LogManager da E14.
CRÍTICO verificar: (1) log_write() sem I/O de SD (só copia para buffer)? (2) flush em task separada (não inline)? (3) buffer cheio descarta antigo (não bloqueia)? (4) SD ausente não causa crash?
Listar problemas.
```

## ✎ Prompt de Correção

```
LogManager com problema: [sintoma — ex: FPS da face cai ao logar]
Contexto: E14, LogManager, flush para SD.
Diagnosticar. A latência de log_write() deve ser <100µs — onde está o blocking?
```

## → Prompt de Continuidade

```
E14 concluída. StorageManager e LogManager com buffer + flush periódico.
Próxima: E15 (Watchdog, boot sequence e PowerManager).
Mostre como configurar o hardware WDT e o task WDT do ESP-IDF, e como implementar uma boot sequence que inicializa componentes em ordem com logging de cada passo.
```


