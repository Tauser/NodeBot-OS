# E10 - Bring-up: microSD e LEDs WS2812

- Status: ✅ Critérios atendidos
- Complexidade: Baixa
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P2
- Risco: BAIXO
- Depende de: 

## Bring-up: microSD e LEDs WS2812

Complexidade: Baixa
Grupo: Bring-up HW
HW Real: SIM
ID: E10
Prioridade: P2
Risco: BAIXO
Status: ✅ Critérios atendidos

## Objetivo

microSD montando FAT32 e lendo/escrevendo arquivo. WS2812 ciclando cores via RMT.

## Critérios de pronto

- Arquivo escrito e relido sem corrupção
- SD ausente: retorna ESP_ERR_NOT_FOUND sem hard fault
- LEDs: cores corretas sem flickering

## Testes mínimos

- Escrever 1000 bytes, reler, comparar byte a byte
- Remover SD, chamar init_fatfs(), verificar retorno de erro sem crash
- LEDs: verificar cor correta com câmera (R/G/B às vezes têm ordem diferente no WS2812B)

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, microSD onboard em SDMMC 1-bit (CMD=38, CLK=39, DATA0=40), RMT peripheral para WS2812.
Tarefa A: sd_driver.h + sd_driver.c.
Funções: sd_init() → esp_err_t, sd_write_file(path, data, len), sd_read_file(path, buf, maxlen) → size_t, sd_file_exists(path) → bool.
Tarefa B: ws2812_driver.h + ws2812_driver.c.
Funções: ws2812_init(gpio, num_leds), ws2812_set_pixel(idx, r, g, b), ws2812_show().
Config WS2812: usar RMT peripheral, timing padrão WS2812B.
Saída: ambos os drivers + testes separados.
```

## ◎ Prompt de Revisão

```
Drivers SD e WS2812 da E10.
Verificar: (1) SD ausente retorna erro (não crash)? (2) SD usa SDMMC onboard 1-bit com os pinos corretos (não SPI do display)? (3) WS2812 usa RMT (não bit-banging)? (4) ws2812_set_pixel() só atualiza buffer; ws2812_show() envia?
Listar problemas.
```

## ✎ Prompt de Correção

```
[SD / WS2812] com problema: [sintoma]
Contexto: E10.
Diagnosticar. Causa + fix.
```

## → Prompt de Continuidade

```
E10 concluída. SD e LEDs funcionando.
Próxima: E11 (validação de consumo e brownout) — última etapa de bring-up antes do runtime.
Mostre como medir corrente de pico do ESP32-S3 com WiFi ativo + servo em movimento + display, e quando um capacitor de hold-up é necessário na linha 3.3V.
```


