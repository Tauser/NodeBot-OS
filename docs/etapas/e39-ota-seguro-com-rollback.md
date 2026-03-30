# E39 - OTA Seguro com Rollback

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Produto
- HW Real: SIM
- Prioridade: P3
- Risco: ALTO
- Depende de: 

## OTA Seguro com Rollback

Complexidade: Alta
Grupo: Produto
HW Real: SIM
ID: E39
Prioridade: P3
Risco: ALTO
Status: 🔲 Não iniciada

## Objetivo

OTA com partições A/B, verificação de assinatura ECDSA e rollback automático. Portão de entrada para qualquer distribuição do produto.

## Janela de segurança obrigatória

```
battery_pct > 30%
AND servos parados
AND não durante conversação ativa
```

## Configuração obrigatória no sdkconfig

```
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y
CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK=n
```

## Critérios de pronto

- 20 OTAs com firmware válido: 100% de sucesso
- 10 OTAs com assinatura inválida: 100% rejeitados
- Rollback: firmware sem heartbeat → rollback confirmado em ≤ 60s
- Janela: OTA com bateria a 20% → rejeitado

## Testes mínimos

- OTA válido: update completo, verificar nova versão rodando
- OTA inválido: assinatura corrompida → deve rejeitar
- Rollback: firmware que não envia heartbeat → rollback em ≤ 60s
- Janela: bateria a 15% → OTA rejeitado com log

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, ESP-IDF OTA, partições A/B. E39 — OTAManager.
Tarefa: ota_manager.h/.c.
ota_manager_check_and_apply(url): (1) verificar janela de segurança; (2) download para partição inativa via esp_https_ota; (3) verificar assinatura ECDSA antes do swap; (4) se OK: esp_ota_set_boot_partition() + esp_restart(); (5) se falha: log + abort.
Rollback: no boot do novo firmware, chamar ota_manager_mark_stable() após 60s de operação OK. Se não chamado: bootloader reverte.
ota_manager_send_heartbeat(): chamar do BehaviorLoop a cada 10s após update.
Saída: ota_manager.h/.c + configuração de sdkconfig para rollback.
```

## ◎ Prompt de Revisão

```
OTAManager da E39.
CRÍTICO verificar: (1) assinatura verificada ANTES do swap (não depois)? (2) rollback configurado no sdkconfig? (3) janela de segurança implementada? (4) OTA nunca automático?
Listar riscos com severidade.
```

## ✎ Prompt de Correção

```
OTA com problema: [sintoma — ex: rollback não acontece após crash no novo firmware]
Contexto: E39.
Verificar configuração do bootloader ESP-IDF para rollback automático.
```

## → Prompt de Continuidade

```
E39 concluída. OTA seguro com rollback.
Próxima: E40 (factory reset, smoke tests e provisionamento).
Mostre como implementar factory reset robusto e smoke test suite que valida todos os periféricos em < 60s.
```


