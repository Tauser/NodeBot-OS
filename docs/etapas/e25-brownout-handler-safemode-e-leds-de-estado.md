# E25 - Brownout Handler, SafeMode e LEDs de Estado

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Safety
- HW Real: SIM
- Prioridade: P1
- Risco: ALTO
- Depende de: 

## Brownout Handler, SafeMode e LEDs de Estado

Complexidade: Alta
Grupo: Safety
HW Real: SIM
ID: E25
Prioridade: P1
Risco: ALTO
Status: 🔲 Não iniciada

## Objetivo

Brownout handler salvando estado antes do reset em < 5ms. SafeMode ativando após 3 boots com falha consecutivos. LEDs comunicando estado do sistema — não desabilitáveis por código de alto nível.

## Estados de LED (não desabilitáveis)

| Estado | Cor |
| --- | --- |
| Normal | 🟢 Verde |
| Degraded | 🟡 Âmbar |
| Escutando (mic ativo) | 🔴 Vermelho fixo |
| Câmera ativa | 🔴 Vermelho piscante |
| Privacy mode | ⚪ Branco fixo |

## Critérios de pronto

- 10 brownouts forçados: config íntegra após todos
- 3 boots com falha simulada: SafeMode ativo na 4ª tentativa
- LED correto para cada estado — verificar visualmente

## Testes mínimos

- Forçar brownout 10×: verificar boot_count e config íntegros
- Simular 3 boots com falha (não decrementar counter): verificar SafeMode na 4ª boot
- Todos os estados de LED: verificar cor correta para NORMAL, DEGRADED, ESCUTANDO

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, NVS, WS2812 (E10). E25 — brownout + SafeMode + LEDs.
Tarefa A: brownout_handler.c — registrar com esp_register_shutdown_handler():
  1. log_flush_now() (max 3ms total)
  2. config_set_int("unclean_shutdown", 1) + nvs_commit()
  3. incrementar "crash_count" em NVS
Tarefa B: safe_mode_service.h/.c — safe_mode_check() chamado no boot:
  - ler "boot_count" do NVS; incrementar; se ≥ 3: ativar safe mode
  - após 60s de operação OK: decrementar para 0
  - safe mode: emotion_mapper_apply(NEUTRAL), ws2812_set_state(LED_STATE_AMBER), habilitar diagnóstico serial
Tarefa C: estados de LED — LED_STATE_NORMAL (verde), LED_STATE_DEGRADED (âmbar), LED_STATE_LISTENING (vermelho fixo), LED_STATE_PRIVACY (branco), LED_STATE_CAMERA (vermelho piscante).
Saída: brownout_handler.c + safe_mode_service.h/.c + atualização ws2812.
```

## ◎ Prompt de Revisão

```
Brownout handler e SafeMode da E25.
Verificar: (1) brownout handler salva em < 5ms? (2) boot_count decrementa após 60s estável (não imediatamente)? (3) LED de privacidade não pode ser desabilitado por código de alto nível? (4) SafeMode aceita OTA mesmo sem behavior?
Listar problemas.
```

## ✎ Prompt de Correção

```
Brownout handler com problema: [sintoma]
Contexto: E25.
Diagnosticar. Causa + risco em produto.
```

## → Prompt de Continuidade

```
E25 concluída. Safety completa: motion safety, health, brownout, SafeMode, LEDs.
Sistema agora seguro e diagnosticável.
Próximas: E26 (TouchService) e E27 (IMUService) — podem ser feitas em sequência.
Próxima: E26 — TouchService com debounce, zonas calibradas e baseline em NVS.
```


