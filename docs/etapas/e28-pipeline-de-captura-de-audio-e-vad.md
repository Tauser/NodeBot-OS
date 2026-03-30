# E28 - Pipeline de Captura de Áudio e VAD

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Áudio
- HW Real: SIM
- Prioridade: P2
- Risco: MÉDIO
- Depende de: 

## Pipeline de Captura de Áudio e VAD

Complexidade: Alta
Grupo: Áudio
HW Real: SIM
ID: E28
Prioridade: P2
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

AudioCaptureTask no Core 0 com buffer circular de 5s em PSRAM. VAD por energia+ZCR+passa-banda. EVT_VOICE_ACTIVITY publicado. Suppression gate contra eco.

## Critérios de pronto

- 5 minutos de silêncio: ≤ 2 falsos positivos
- Falar 20 palavras distintas: ≥ 18 EVT_VOICE_ACTIVITY(is_speech=true)
- Playback ativo: 0 falsos positivos durante reprodução do próprio robô

## ⚠️ Regra crítica de arquitetura

AudioCaptureTask **obrigatoriamente no Core 0** — nunca no Core 1. Competiria com FaceRender e quebraria o FPS.

## Testes mínimos

- 5 minutos silêncio: contar EVT_VOICE_ACTIVITY(is_speech=true) — meta ≤ 2
- 20 palavras: verificar que ≥ 18 geram EVT(is_speech=true)
- Playback longo: verificar 0 ativações durante reprodução

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, INMP441 (E05), PSRAM para buffer, Core 0 P14. E28 — VAD.
Tarefa: audio_capture.h/.c + vad.h/.c.
AudioCaptureTask (Core 0, P14): ler I2S DMA em blocos de 512 samples (20ms @ 16kHz); escrever em ring_buffer circular de 80 blocos (5s total em PSRAM).
VAD por bloco de 20ms:
  rms = sqrt(sum(x²)/N); zcr = count_zero_crossings(block)/N;
  is_speech = (rms > rms_threshold) AND (zcr > 0.05) AND (zcr < 0.45);
  rms_threshold: lido do NVS, default 200 (de 32767).
Suppression gate: vad_suppress_ms(200) chamado quando playback inicia.
Publicar EVT_VOICE_ACTIVITY{energy_db=20*log10(rms/32767), is_speech} a cada bloco onde is_speech muda.
Saída: audio_capture.h/.c + vad.h/.c.
```

## ◎ Prompt de Revisão

```
VAD da E28.
Verificar: (1) AudioCaptureTask está no Core 0? (2) suppression gate implementado? (3) threshold lido do NVS (não hardcoded)? (4) buffer circular sem malloc em runtime? (5) CPU Core 0 ≤ 20%?
Listar problemas.
```

## ✎ Prompt de Correção

```
VAD com problema: [sintoma — ex: falsos positivos com ruído de servo]
Contexto: E28.
Diagnosticar. Causa + fix.
```

## → Prompt de Continuidade

```
E28 concluída. Captura no Core 0, VAD com suppression gate.
Próxima: E29 (AudioFeedback e playback).
Mostre como implementar AudioFeedbackTask com fila de comandos e como organizar os arquivos de som pré-gravados no SD.
```


