# E30 - Wake Word Local — ESP-SR WakeNet

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Áudio
- HW Real: SIM
- Prioridade: P2
- Risco: MÉDIO
- Depende de: 

## Wake Word Local — ESP-SR WakeNet

Complexidade: Alta
Grupo: Áudio
HW Real: SIM
ID: E30
Prioridade: P2
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

Wake word detectado localmente via ESP-SR WakeNet, rodando no Core 0 dentro da AudioCaptureTask. Supressão de eco implementada. Reação facial em < 600ms.

## Critérios de pronto

- 50 pronunciações: ≥ 42 detectadas (≥ 85%)
- TV por 1h: ≤ 3 falsos positivos
- Robô fala resposta longa: 0 auto-ativações
- Privacy mode ativo: 0 detecções em 20 tentativas

## Sequência de reação ao wake word

1. T+0ms — detecção
2. T+50ms — LED vermelho aceso
3. T+80ms — audio_feedback_play(WHOOSH_ACTIVATE)
4. T+100ms — EVT_WAKE_WORD publicado
5. T+200ms — face muda para SURPRISED/ENGAGED

## Testes mínimos

- 50 pronunciações: contar detecções (meta ≥ 85%)
- 1h de TV: contar falsos positivos (meta ≤ 3)
- 20 tentativas com privacy mode: verificar 0 detecções
- 10 falas longas do robô: verificar 0 auto-ativações

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, ESP-SR disponível, AudioCaptureTask no Core 0 (E28). E30 — Wake word.
Tarefa: wake_word.h + wake_word.c — integrar ESP-SR WakeNet como sub-módulo da AudioCaptureTask.
Inicialização: wake_word_init(model_data, model_size) — carregar modelo WakeNet em PSRAM.
Processamento: wake_word_feed(int16_t *block, size_t samples) → confidence ou 0.
Chamar no AudioCaptureTask a cada bloco (512 samples). Se confidence > threshold_nv: publicar EVT_WAKE_WORD{confidence, timestamp_ms}.
Supressão: wake_word_suppress_ms(800) — não publicar por 800ms após início de qualquer playback.
Privacy: verificar privacy_policy_is_active() antes de publicar.
Saída: wake_word.h/.c + como integrar em audio_capture.c.
```

## ◎ Prompt de Revisão

```
Wake word da E30.
Verificar: (1) roda no Core 0 (não Core 1)? (2) supressão de eco implementada (800ms após playback)? (3) privacy mode bloqueia antes de qualquer processamento? (4) threshold configurável via NVS?
Listar problemas.
```

## ✎ Prompt de Correção

```
Wake word com problema: [sintoma — ex: detecção caindo para 50% com ruído]
Contexto: E30, ESP-SR.
Diagnosticar. Causa + como melhorar threshold ou modelo.
```

## → Prompt de Continuidade

```
E30 concluída. Wake word com ≥ 85% de detecção, auto-ativação zero.
Próxima: E31 (comandos offline mínimos — keyword spotting e intent mapper).
Mostre como implementar DTW template matching para 12 comandos offline e como definir o IntentMapper.
```


