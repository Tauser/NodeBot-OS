# E37 - CloudBridge — STT, TTS Cloud e Timeouts

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Câmera/Cloud
- HW Real: SIM
- Prioridade: P3
- Risco: MÉDIO
- Depende de: 

## CloudBridge — STT, TTS Cloud e Timeouts

Complexidade: Alta
Grupo: Câmera/Cloud
HW Real: SIM
ID: E37
Prioridade: P3
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

CloudBridge não-bloqueante com política de timeout oficial. Cloud é enhancement — produto funciona sem ele e o usuário nunca espera.

## Política de timeout oficial

| Timeout | Valor | Ação |
| --- | --- | --- |
| T_response_local | 300 ms | Resposta local sempre disponível |
| T_cloud_soft | 600 ms | Inicia fallback local como primário |
| T_cloud_hard | 1200 ms | Aborta request, loga cloud_timeout |

## Critérios de pronto

- WiFi off: resposta local em ≤ 300ms — nunca espera cloud
- T_soft test: simular latência de 800ms → fallback local em 600ms
- Cache: mesma pergunta 2×: segunda via cache sem request cloud
- T_hard: simular latência de 2000ms → request abortado em ≤ 1200ms

## Testes mínimos

- WiFi off: verificar resposta local em ≤ 300ms
- T_soft: simular latência 800ms → fallback em 600ms
- T_hard: simular 2000ms → abort em ≤ 1200ms + cloud_timeout log

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, WiFi disponível, DialogueStateService (E32). E37 — CloudBridge.
Tarefa: cloud_bridge.h/.c.
cloud_bridge_request_stt(int16_t *audio, size_t len, void (*cb)(const char *text)): enfileira request para CloudTask (Core 0, P8). Não bloqueia.
Política de timeout: T_local=300ms, T_soft=600ms, T_hard=1200ms conforme tabela.
CloudTask: HTTP POST para STT API; ao receber resposta: verificar se dentro do prazo; se sim, invocar cb(); se não, log "cloud_timeout_dropped".
WiFiManager: wifi_manager_is_connected() → bool; wifi_manager_start_duty_cycle(on_ms=500, off_ms=5000).
Saída: cloud_bridge.h/.c + wifi_manager.h/.c.
```

## ◎ Prompt de Revisão

```
CloudBridge da E37.
CRÍTICO verificar: (1) cloud_bridge_request_*() retorna imediatamente? (2) T_hard=1200ms realmente aplicado (não apenas T_soft)? (3) resposta local já saiu antes de cloud tentar? (4) WiFi duty cycle evita brownout?
Listar problemas.
```

## ✎ Prompt de Correção

```
CloudBridge com problema: [sintoma — ex: BehaviorLoop congela esperando resposta]
Contexto: E37.
Causa: chamada síncrona ou bloqueante. Fix: arquitetura assíncrona.
```

## → Prompt de Continuidade

```
E37 concluída. Cloud como enhancement, timeouts corretos, fallback gracioso.
Câmera e Cloud COMPLETOS.
Próxima: E38 (observabilidade completa).
Mostre o que adicionar ao LogManager da E14 para ter snapshots de estado, contadores de falha e modo de diagnóstico via serial.
```


