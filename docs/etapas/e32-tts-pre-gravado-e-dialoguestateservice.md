# E32 - TTS Pré-gravado e DialogueStateService

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Áudio
- HW Real: SIM
- Prioridade: P2
- Risco: MÉDIO
- Depende de: 

## TTS Pré-gravado e DialogueStateService

Complexidade: Média
Grupo: Áudio
HW Real: SIM
ID: E32
Prioridade: P2
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

TTS por áudio pré-gravado para 30+ frases. DialogueStateService com FSM completo e timeouts. Primeira conversa offline de ponta a ponta.

## FSM do DialogueStateService

```
IDLE + EVT_WAKE_WORD → LISTENING (timer 3s)
LISTENING + EVT_INTENT_DETECTED → PROCESSING (timer 5s)
LISTENING timeout → IDLE + play_phrase(PHRASE_TIMEOUT_LISTENING)
PROCESSING + resposta → SPEAKING
PROCESSING timeout → IDLE + play_phrase(PHRASE_NOT_UNDERSTOOD)
SPEAKING + playback_done → IDLE
```

## Critérios de pronto

- Todos os 5 estados: verificar transições corretas
- Timeout de LISTENING: 3s sem fala → volta a IDLE
- Timeout de PROCESSING: 5s → INTENT_UNKNOWN → frase de fallback
- LED vermelho: aceso exatamente durante estado LISTENING

## Testes mínimos

- Percorrer todos os estados: confirmar cada transição correta
- Timeout: esperar 4s após wake word sem falar — robô deve voltar a IDLE
- Frase dinâmica: pedir "que horas são" e verificar que hora é falada corretamente

---

## ▶ Prompt Principal

```
Contexto: E30 (wake word), E31 (intents), E29 (playback). E32 — DialogueStateService.
Tarefa A: dialogue_state_service.h/.c — FSM com estados e transições conforme tabela acima.
Publicar EVT_DIALOGUE_STATE_CHANGED a cada transição.
Tarefa B: tts.h/.c — tts_play_phrase(phrase_id), tts_play_dynamic(TPL_HOUR, int val).
Fallback: SD ausente → play_phrase usa BEEP_ACK + frases genéricas em flash.
Saída: dialogue_state_service.h/.c + tts.h/.c.
```

## ◎ Prompt de Revisão

```
DialogueStateService da E32.
CRÍTICO verificar: (1) timeout em TODOS os estados (nenhum pode ficar preso)? (2) LED correto em cada estado? (3) playback_done notificado de volta ao FSM? (4) FSM thread-safe?
Listar problemas.
```

## ✎ Prompt de Correção

```
DialogueStateService travado em [ESTADO].
Contexto: E32.
Identificar: timeout não disparando ou transição missing. Causa + fix.
```

## → Prompt de Continuidade

```
E32 concluída. TTS + DialogueState. Primeira conversa offline completa.
Pipeline de áudio COMPLETO.
Próxima: E33 (BehaviorEngine loop + behavior tree) — integração de todos os subsistemas.
Mostre como implementar o BehaviorEngine como loop de 10Hz que lê o StateVector, executa behavior tree e publica FaceCommand e AudioCommand.
```


