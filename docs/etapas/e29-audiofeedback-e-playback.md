# E29 - AudioFeedback e Playback

- Status: 🔲 Não iniciada
- Complexidade: Baixa
- Grupo: Áudio
- HW Real: SIM
- Prioridade: P2
- Risco: BAIXO
- Depende de: 

## AudioFeedback e Playback

Complexidade: Baixa
Grupo: Áudio
HW Real: SIM
ID: E29
Prioridade: P2
Risco: BAIXO
Status: 🔲 Não iniciada

## Objetivo

AudioPlaybackTask com fila de comandos. audio_feedback_play() não bloqueante (retorna em < 1ms). 5 sons pré-gravados no SD com fallback em flash.

## Sons obrigatórios

- BEEP_ACK — confirmação de ação
- DING_NOTIF — notificação
- WHOOSH_ACTIVATE — ativação de wake word
- CLICK_TOUCH — resposta a toque
- ERROR_TONE — erro / não entendeu

## Critérios de pronto

- audio_feedback_play() retorna em < 1ms em 100 chamadas
- 5 sons distintos audíveis sem distorção
- SD ausente: audio_feedback_play() usa som fallback em flash

## Testes mínimos

- audio_feedback_play() 100×: verificar retorno em < 1ms cada
- SD ausente: chamar play() e verificar que não crasha
- Qualidade: ouvir cada som subjetivamente

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, MAX98357A (E06), SD (E10), PSRAM. E29 — AudioFeedback.
Tarefa: audio_feedback.h/.c + audio_playback_task.h/.c.
AudioPlaybackTask: Core 1, P18, aguarda na AudioCommandQueue.
audio_feedback_init(): carregar 5 sons do SD para PSRAM: {BEEP_ACK, DING_NOTIF, WHOOSH_ACTIVATE, CLICK_TOUCH, ERROR_TONE}.
audio_feedback_play(sound_id_t id): enfileira AudioCommand na AudioCommandQueue — NUNCA BLOQUEIA, retorna em < 1ms.
AudioPlaybackTask: ao receber comando, reproduz PCM via max98357a_play_pcm(). Não inicia novo som até atual terminar.
Fallback SD ausente: BEEP_ACK gerado em software (440Hz, 100ms) armazenado como constante em flash.
Saída: audio_feedback.h/.c + audio_playback_task.h/.c.
```

## ◎ Prompt de Revisão

```
AudioFeedback da E29.
Verificar: (1) play() não bloqueia (retorna em < 1ms)? (2) sons carregados no init (não durante playback)? (3) fallback em flash para SD ausente? (4) volume master configurável via NVS?
Listar problemas.
```

## ✎ Prompt de Correção

```
AudioFeedback com problema: [sintoma]
Contexto: E29.
Causa + fix.
```

## → Prompt de Continuidade

```
E29 concluída. AudioFeedback com 5 sons, playback não bloqueante.
Próxima: E30 (Wake Word Local — ESP-SR WakeNet).
Mostre como integrar ESP-SR WakeNet à AudioCaptureTask existente (Core 0) e como configurar threshold via NVS.
```


