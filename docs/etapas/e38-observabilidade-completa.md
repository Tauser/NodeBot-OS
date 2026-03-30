# E38 - Observabilidade Completa

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Produto
- HW Real: SIM
- Prioridade: P3
- Risco: BAIXO
- Depende de: 

## Observabilidade Completa

Complexidade: Média
Grupo: Produto
HW Real: SIM
ID: E38
Prioridade: P3
Risco: BAIXO
Status: 🔲 Não iniciada

## Objetivo

Logs estruturados, snapshots periódicos de estado, contadores de falha em NVS e modo de diagnóstico. Produto diagnosticável em campo sem hardware de debug.

## Critérios de pronto

- Crash intencionalmente forçado: logs no SD descrevem o serviço e estado antes do crash
- DiagnosticMode ativo: face continua respondendo (não bloqueia)
- Exportação: pacote de debug gerado em < 30s
- Contadores: após 5 crashes forçados, crash_count = 5 no NVS

## Testes mínimos

- 5 crashes diferentes: verificar que cada um aparece no log com contexto suficiente
- DiagnosticMode: ativar com face respondendo — verificar que face não trava
- Exportação: pacote gerado e verificado tamanho e conteúdo

---

## ▶ Prompt Principal

```
Contexto: LogManager (E14), HealthMonitor (E24), EventBus (E12). E38 — observabilidade.
Tarefa A: snapshot_service.h/.c — a cada 60s, capturar StateVector + contadores + FPS + heap livre em snapshot_{timestamp}.json no SD.
Tarefa B: diagnostic_mode.h/.c — ativar via 3× ZONE_TOP em < 2s. No modo ativo: imprimir a cada 2s via serial JSON com {fps, cpu_percent, heap_free, battery_pct, crash_count, mood_valence, mood_arousal}. Desativar após 60s ou outro toque triplo.
Tarefa C: atualizar contadores em NVS — incrementar crash_count em brownout_handler e service_restart_count no HealthMonitor.
TelemetryService stub: estrutura pronta, envio desabilitado por padrão (opt-in).
Saída: snapshot_service + diagnostic_mode + atualização de brownout_handler.c.
```

## ◎ Prompt de Revisão

```
Observabilidade da E38.
Verificar: (1) snapshot não escreve a cada tick (apenas a cada 60s)? (2) DiagnosticMode não bloqueia FaceRender? (3) TelemetryService desabilitado por default? (4) pacote de debug exportado em < 30s?
Listar problemas.
```

## ✎ Prompt de Correção

```
Observabilidade com problema: [sintoma — ex: DiagnosticMode trava a face]
Contexto: E38.
Causa + fix para tornar assíncrono.
```

## → Prompt de Continuidade

```
E38 concluída. Observabilidade completa e diagnóstico funcional.
Próxima: E39 (OTA seguro com rollback).
Mostre como configurar partições A/B no ESP32-S3, implementar OTA com verificação ECDSA e rollback automático se heartbeat não chegar em 60s.
```


