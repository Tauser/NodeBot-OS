# E41 - Jig de Fábrica e Industrialização Inicial

- Status: 🔲 Não iniciada
- Complexidade: Muito Alta
- Grupo: Produto
- HW Real: SIM
- Prioridade: P4
- Risco: MÉDIO
- Depende de: 

## Jig de Fábrica e Industrialização Inicial

Complexidade: Muito Alta
Grupo: Produto
HW Real: SIM
ID: E41
Prioridade: P4
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

Jig de teste automatizado em Python testando todos os periféricos em < 60s. Processo de provisionamento de fábrica. BOM de produção com 3 cenários de volume.

## Testes do jig (sequenciais, 5s de timeout cada)

| Teste | Comando enviado | Critério de PASS |
| --- | --- | --- |
| boot | — | "BOOT_OK" em 5s |
| display | TEST_DISPLAY | "DISPLAY_OK" |
| servo | TEST_SERVO | "SERVO_OK" (move e retorna posição) |
| mic | TEST_MIC | "MIC_OK rms=[N]" onde N > 50 |
| speaker | TEST_SPEAKER | "SPEAKER_OK" |
| SD | TEST_SD | "SD_OK" |
| battery | TEST_BATTERY | "BATTERY_OK pct=[N]" onde N > 0 |
| IMU | TEST_IMU | "IMU_OK mag=[N]" onde N entre 900–1100 |
| LED | TEST_LED | "LED_OK" |
| WiFi | TEST_WIFI | "WIFI_OK networks=[N]" onde N > 0 |

## Critérios de pronto

- Jig: 10 unidades consecutivas todas PASS
- Jig com falha proposital (servo desconectado): FAIL correto no item de servo
- Provisionamento: cronometrado em < 5 minutos por unidade
- BOM: custo calculado para lotes de 50, 200 e 500 unidades

## Testes mínimos

- Jig: 10 unidades com PASS; 5 com falha proposital (FAIL correto)
- Provisionamento: pessoa não-técnica executa seguindo o manual
- Tempo: cronometrar processo completo de provisionamento

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, firmware de fábrica com smoke tests, comunicação serial via USB. E41 — jig_test.py.
Tarefa: jig_test.py em Python.
Para cada teste da tabela acima: enviar comando serial, aguardar resposta com timeout 5s, verificar resultado.
FAIL em um teste não aborta os demais — continuar e reportar todos.
Resultado final: JSON com {serial_id, pass_count, fail_count, details:[{test, result, message}], timestamp}.
Saída: jig_test.py completo + exemplo de execução.
```

## ◎ Prompt de Revisão

```
Jig de fábrica da E41.
Verificar: (1) cada teste tem timeout (não trava)? (2) FAIL em um teste não aborta os demais? (3) resultado é JSON? (4) testa falha proposital corretamente? (5) tempo total < 60s?
Listar problemas.
```

## ✎ Prompt de Correção

```
Jig com problema: [sintoma — ex: test_servo retorna PASS com servo desconectado]
Contexto: E41.
Torne o teste robusto: como verificar que o servo realmente se moveu.
```

## → Prompt de Continuidade

```
E41 concluída. Jig de fábrica, provisionamento e BOM documentados.
GUIA COMPLETO. 41 etapas concluídas.
O produto está pronto para primeira rodada de produção controlada (10–50 unidades).
Próximos passos: certificações (CE/FCC/ANATEL), feedback de campo, iteração com dados reais.
```

---

## 🎉 Marco final

Ao concluir E41, todas as 41 etapas estão completas. O produto tem:

- ✅ Hardware validado periférico por periférico
- ✅ Runtime estável com EventBus, Config, Log, Watchdog
- ✅ Face procedural a 20fps com expressões e gaze
- ✅ Safety completa (MotionSafety, HealthMonitor, Brownout, SafeMode)
- ✅ Pipeline de áudio com wake word e conversação offline
- ✅ BehaviorEngine com personagem coerente
- ✅ Memória, mood e atenção
- ✅ Câmera e cloud como enhancement opcional
- ✅ OTA seguro, factory reset e smoke tests
- ✅ Jig de fábrica e processo de industrialização


