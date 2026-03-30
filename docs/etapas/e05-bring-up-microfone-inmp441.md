# E05 - Bring-up: Microfone INMP441

- Status: ✅ Critérios atendidos
- Complexidade: Média
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P1
- Risco: MÉDIO
- Depende de: E02

## Bring-up: Microfone INMP441

Complexidade: Média
Depende de: E02
Grupo: Bring-up HW
HW Real: SIM
ID: E05
Prioridade: P1
Risco: MÉDIO
Status: ✅ Critérios atendidos

## Objetivo

INMP441 lendo amostras de áudio via I2S. Buffer circular funcional. Nível de sinal verificado com voz real.

## Critérios de pronto

- RMS em silêncio < 100 (de 32767)
- RMS em voz normal 500–5000
- Buffer de 1s sem overflow em 10 minutos contínuos

## Testes mínimos

- Plotar 1s de silêncio no serial — verificar RMS < 100
- Falar no microfone, verificar que RMS sobe visivelmente
- Ligar servo próximo e verificar impacto no sinal

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, I2S port 0, INMP441 PDM, 16kHz, hal_init.h com pinos I2S_MIC_*.
Tarefa: inmp441_driver.h + inmp441_driver.c.
Funções: inmp441_init(), inmp441_read_samples(int16_t *buf, size_t samples) → retorna amostras lidas.
Config: I2S_COMM_FORMAT_STAND_I2S, 32-bit container, 16-bit dados (shift de 16 bits para extrair amostra útil), mono.
Buffer DMA: 4 buffers de 512 samples.
Saída: driver + teste que imprime RMS a cada 500ms via serial.
```

## ◎ Prompt de Revisão

```
Driver INMP441 da E05.
Verificar: (1) shift de 16 bits aplicado (dado útil nos bits 15:0 do container 32-bit)? (2) sem DC offset >200 em silêncio? (3) buffer DMA sem overflow em 5 minutos? (4) sample rate exatamente 16kHz?
Listar problemas.
```

## ✎ Prompt de Correção

```
Microfone com problema: [sintoma — ex: amostras todas zeradas]
Contexto: E05, INMP441, I2S PDM, ESP32-S3.
Verificar: WS polarity, SCK freq, data shift. Saída: causa + fix.
```

## → Prompt de Continuidade

```
E05 concluída. Microfone lendo amostras, RMS documentado.
Próxima: E06 (bring-up de áudio playback com MAX98357A).
Mostre como configurar I2S para MAX98357A no ESP32-S3 e como tocar um beep de 440Hz por 200ms.
```


