# E06 - Bring-up: Áudio Playback MAX98357A

- Status: ✅ Critérios atendidos
- Complexidade: Baixa
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P1
- Risco: BAIXO
- Depende de: E05

## Bring-up: Áudio Playback MAX98357A

Complexidade: Baixa
Depende de: E05
Grupo: Bring-up HW
HW Real: SIM
ID: E06
Prioridade: P1
Risco: BAIXO
Status: ✅ Critérios atendidos

## Objetivo

MAX98357A tocando áudio via I2S. Beep de teste sem distorção. Volume configurável. I2S de captura e playback simultâneos sem interferência.

## Critérios de pronto

- Beep audível e limpo — sem clipping, sem ruído de fundo perceptível
- Captura e playback simultâneos: amostras de captura não corrompidas durante playback
- Volume a 50%: nível confortável (~70dB SPL a 30cm)

## Testes mínimos

- Tocar beep 440Hz e avaliar subjetivamente (sem distorção)
- Captura + playback simultâneos: verificar que RMS do microfone não muda durante playback
- Testar 3 níveis de volume (25%, 50%, 100%)

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, I2S port 1 (separado do microfone), MAX98357A, hal_init.h com pinos I2S_SPK_*.
Tarefa: max98357a_driver.h + max98357a_driver.c.
Funções: max98357a_init(), max98357a_play_pcm(int16_t *buf, size_t samples), max98357a_set_volume(uint8_t 0–100).
Config: 16kHz, 16-bit, mono, I2S_MODE_MASTER_TX.
Incluir: função generate_beep(freq_hz, duration_ms, int16_t *out_buf) → size_t samples para teste.
Saída: driver completo + teste que toca beep 440Hz por 200ms.
```

## ◎ Prompt de Revisão

```
Driver MAX98357A da E06.
Verificar: (1) I2S port diferente do microfone? (2) sem bloqueio no play_pcm() (usa DMA)? (3) beep audível sem distorção? (4) captura simultânea não corrompida?
Listar problemas.
```

## ✎ Prompt de Correção

```
Áudio playback com problema: [sintoma]
Contexto: E06, MAX98357A, I2S, ESP32-S3.
Diagnosticar. Saída: causa + fix.
```

## → Prompt de Continuidade

```
E06 concluída. Playback funcionando, beep sem distorção.
Próxima: E07 (bring-up de bateria e power path: MAX17048, bq25185, TPS61088).
Mostre como ler SOC% e tensão do MAX17048 via I2C no ESP32-S3.
```


