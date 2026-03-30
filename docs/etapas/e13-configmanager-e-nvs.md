# E13 - ConfigManager e NVS

- Status: ✅ Critérios atendidos
- Complexidade: Média
- Grupo: Runtime
- HW Real: SIM
- Prioridade: P1
- Risco: MÉDIO
- Depende de: 

## ConfigManager e NVS

Complexidade: Média
Grupo: Runtime
HW Real: SIM
ID: E13
Prioridade: P1
Risco: MÉDIO
Status: ✅ Critérios atendidos

## Objetivo

ConfigManager lendo e escrevendo configuração via NVS com schema versionado, CRC de integridade e defaults seguros em flash.

## Critérios de pronto

- Config persiste após 10 reboots sem alteração
- NVS corrompido (forçar): detectado, defaults carregados, zero crash
- get_int(chave_inexistente): retorna default, não crash

## Testes mínimos

- Gravar config, reiniciar 10×, verificar que valores persistem
- Corromper NVS manualmente (nvs_flash_erase), verificar que defaults são carregados
- Chamar get_int com chave inexistente: deve retornar default sem crash

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, ESP-IDF NVS, projeto robô desktop. E13 — ConfigManager.
Tarefa: config_manager.h + config_manager.c.
Interface:
  esp_err_t config_manager_init(void);  // carrega NVS, verifica CRC, fallback para defaults se inválido
  int32_t   config_get_int(const char *key, int32_t default_val);
  esp_err_t config_set_int(const char *key, int32_t val);  // escreve só se mudou; atualiza CRC
  esp_err_t config_factory_reset(void);  // apaga NVS, carrega nvs_defaults.h
  uint16_t  config_get_schema_version(void);
CRC: crc32 de todo o namespace NVS, armazenado em chave "crc32".
Saída: implementação + nvs_defaults.h com 10 chaves de exemplo + teste.
```

## ◎ Prompt de Revisão

```
ConfigManager da E13.
Verificar: (1) CRC calculado a cada set() e verificado no init()? (2) defaults em flash (não NVS)? (3) set() só escreve NVS quando valor realmente mudou? (4) get() com chave inexistente retorna default (não crash)?
Listar problemas.
```

## ✎ Prompt de Correção

```
ConfigManager com problema: [sintoma]
Contexto: E13.
Diagnosticar. Causa + fix.
```

## → Prompt de Continuidade

```
E13 concluída. ConfigManager com NVS, CRC e defaults seguros.
Próxima: E14 (StorageManager e LogManager).
Mostre como abstrair SD + NVS no StorageManager e como implementar LogManager com buffer circular em RAM e flush periódico para SD.
```


