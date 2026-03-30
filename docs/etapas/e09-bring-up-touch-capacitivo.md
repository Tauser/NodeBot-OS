# E09 - Bring-up: Touch Capacitivo

- Status: 🔄 Em progresso
- Complexidade: Baixa
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P2
- Risco: BAIXO
- Depende de: E02

## Bring-up: Touch Capacitivo

Complexidade: Baixa
Depende de: E02
Grupo: Bring-up HW
HW Real: SIM
ID: E09
Prioridade: P2
Risco: BAIXO
Status: 🔄 Em progresso

## Objetivo

Fita de cobre detectando toque com leitura via touch peripheral ou ADC. Baseline de ruído medido. 1 zona identificada.

## Critérios de pronto

- Cada zona detecta toque em < 20ms (raw read)
- 0 falsos positivos em 2 minutos de monitoramento sem toque
- Toque em zona A não aciona zona B (sem cross-talk)

## Testes mínimos

- 2 minutos sem toque: contar falsos positivos (meta: 0)
- Tocar cada zona 20 vezes: confirmar detecção
- Medir latência: tempo entre toque físico e is_touched() retornando true

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, fita de cobre em 1 zona (top). Usar touch peripheral nativo do ESP32-S3 se GPIO disponível; caso contrário, ADC.
Tarefa: touch_driver.h + touch_driver.c.
Funções: touch_driver_init(), touch_driver_read_raw(zone_id) → uint32_t, touch_driver_is_touched(zone_id) → bool.
Calibração: touch_driver_calibrate() lê 100 amostras por zona, threshold = média × 0.75.
Saída: driver + teste que imprime "TOUCHED zone [N]" a cada detecção.
```

## ◎ Prompt de Revisão

```
Touch driver da E09.
Verificar: (1) usa touch peripheral nativo (mais confiável que ADC)? (2) threshold calculado automaticamente (não hardcoded)? (3) calibrate() chamado antes de is_touched()? (4) cross-talk entre zonas testado?
Listar problemas.
```

## ✎ Prompt de Correção

```
Touch com problema: [sintoma]
Contexto: E09, ESP32-S3.
Diagnosticar. Causa + fix.
```

## → Prompt de Continuidade

```
E09 concluída. Touch detectando 4 zonas, baseline medido.
Próxima: E10 (bring-up de microSD e LEDs WS2812).
Mostre como montar FAT32 no microSD com ESP-IDF e como controlar WS2812 via RMT peripheral.
```


