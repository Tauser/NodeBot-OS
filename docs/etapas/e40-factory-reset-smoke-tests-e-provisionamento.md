# E40 - Factory Reset, Smoke Tests e Provisionamento

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Produto
- HW Real: SIM
- Prioridade: P3
- Risco: MÉDIO
- Depende de: 

## Factory Reset, Smoke Tests e Provisionamento

Complexidade: Média
Grupo: Produto
HW Real: SIM
ID: E40
Prioridade: P3
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

Factory reset completo. Smoke test suite automatizada passando em < 60s. Processo de provisionamento de primeiro uso documentado.

## Critérios de pronto

- Factory reset: NVS e SD limpos após reset, boot para defaults sem crash
- Smoke tests: 10 execuções consecutivas com 100% de pass
- Soak 100h: zero crashes, heap livre estável

## Testes mínimos

- Factory reset: executar e verificar NVS e SD limpos
- Smoke tests: executar 10× e verificar 100% de pass
- Smoke com falha proposital: desconectar servo → smoke deve retornar FAIL no item de servo

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, todos os sistemas E01–E39. E40 — factory reset e smoke tests.
Tarefa A: factory_reset.h/.c.
factory_reset_trigger(): confirmação por botão físico por 5s; se confirmado: nvs_flash_erase_partition_type(ALL); sd_format(); esp_restart().
Indicador: LED piscando âmbar durante 5s de confirmação; vermelho fixo durante apagamento; verde ao reiniciar.
Tarefa B: smoke_tests.c — apenas compilado com -DQA_BUILD.
smoke_test_run() → smoke_result_t: testar em sequência: display_ok, servo_ok, mic_ok, speaker_ok, sd_ok, battery_ok, imu_ok, led_ok, touch_ok, wifi_ok.
Cada teste: timeout 5s; PASS se periférico responde; FAIL com mensagem descritiva.
Total: < 60s. Resultado: JSON no serial + LED verde (all pass) ou vermelho (any fail).
Saída: factory_reset.h/.c + smoke_tests.c.
```

## ◎ Prompt de Revisão

```
Factory reset e smoke tests da E40.
Verificar: (1) factory reset requer confirmação física (não só software)? (2) smoke tests têm timeout por item? (3) FAIL em item não aborta os demais? (4) resultado em JSON?
Listar problemas.
```

## ✎ Prompt de Correção

```
Smoke test com problema: [sintoma — ex: smoke retorna PASS com servo desconectado]
Contexto: E40.
Como tornar o teste robusto para detectar periférico ausente?
```

## → Prompt de Continuidade

```
E40 concluída. Factory reset, smoke tests e soak 100h concluídos.
Produto pronto para distribuição controlada.
Próxima e última: E41 (jig de fábrica e industrialização inicial).
Mostre a estrutura do script jig_test.py em Python que testa todas as funções de um robô recém-montado em < 60s.
```


