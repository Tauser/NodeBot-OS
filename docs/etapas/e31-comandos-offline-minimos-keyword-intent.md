# E31 - Comandos Offline Mínimos — Keyword + Intent

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Áudio
- HW Real: SIM
- Prioridade: P2
- Risco: MÉDIO
- Depende de: 

## Comandos Offline Mínimos — Keyword + Intent

Complexidade: Média
Grupo: Áudio
HW Real: SIM
ID: E31
Prioridade: P2
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

12 comandos básicos reconhecidos offline via keyword spotting. IntentMapper traduzindo keywords para intents. Primeiro controle por voz completamente offline.

## Comandos obrigatórios

dorme, acorda, silêncio, modo privado, que horas são, como você está, me olha, volume alto, volume baixo, sim, não, cancela

## Critérios de pronto

- Cada um dos 12 comandos: 10 tentativas → ≥ 8 reconhecimentos corretos (≥ 80%)
- INTENT_UNKNOWN: dizer palavras aleatórias → sempre INTENT_UNKNOWN (não match errado)
- Latência: comando falado → EVT_INTENT_DETECTED ≤ 500ms após fim da fala

## Testes mínimos

- 12 comandos × 10 tentativas: medir acurácia por comando
- Palavras aleatórias: verificar que retornam INTENT_UNKNOWN
- Latência: timestamp no log de EVT_WAKE_WORD e EVT_INTENT_DETECTED

---

## ▶ Prompt Principal

```
Contexto: ESP-SR Wake word (E30), EVT_WAKE_WORD disponível. E31 — keyword spotting.
Tarefa A: keyword_spotter.h/.c — DTW template matching.
keyword_spotter_load_templates(path): carregar templates WAV do SD (5 por comando, 12 comandos).
keyword_spotter_match(int16_t *audio, size_t len) → {keyword_id, confidence}.
DTW: extrair MFCCs (13 coefs, 25ms frame, 10ms hop), calcular distância DTW contra cada template, retornar melhor match se dist < threshold.
Tarefa B: intent_mapper.h/.c — tabela keyword_id → intent_t.
Definir intent_t enum com 12 intents + INTENT_UNKNOWN.
intent_mapper_resolve(keyword_id) → intent_t.
Ao receber EVT_WAKE_WORD: capturar 3s de áudio, processar, publicar EVT_INTENT_DETECTED{intent, confidence}.
Saída: keyword_spotter.h/.c + intent_mapper.h/.c.
```

## ◎ Prompt de Revisão

```
Keyword spotting da E31.
Verificar: (1) templates carregados do SD (não hardcoded)? (2) INTENT_UNKNOWN quando dist > threshold? (3) captura de 3s não bloqueia BehaviorLoop? (4) latência total ≤ 500ms?
Listar problemas.
```

## ✎ Prompt de Correção

```
Keyword spotting com problema: [sintoma — ex: acurácia de "dorme" é 40%]
Contexto: E31, DTW.
Como melhorar: mais templates, normalização, threshold?
```

## → Prompt de Continuidade

```
E31 concluída. 12 comandos offline com ≥ 80% de acurácia.
Próxima: E32 (TTS pré-gravado e DialogueStateService).
Mostre como implementar o FSM do DialogueStateService (IDLE→LISTENING→PROCESSING→SPEAKING→IDLE) com timeouts em todos os estados.
```


