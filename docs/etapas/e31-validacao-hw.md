# E31 — Validação em HW: Templates + Testes de Acurácia

> Pré-requisito: build OK, SD montado em `/sdcard`, robot com microfone INMP441 funcionando.

---

## Passo 1 — Gravar os templates WAV

### Keywords e nomes de arquivo

| ID  | Arquivo base | O que falar      |
| --- | ------------ | ---------------- |
| 0   | `dorme`      | "dorme"          |
| 1   | `acorda`     | "acorda"         |
| 2   | `silencio`   | "silêncio"       |
| 3   | `privado`    | "modo privado"   |
| 4   | `que_horas`  | "que horas são"  |
| 5   | `como_voce`  | "como você está" |
| 6   | `olhe_para_mim` | "me olha"     |
| 7   | `vol_alto`   | "volume alto"    |
| 8   | `vol_baixo`  | "volume baixo"   |
| 9   | `sim`        | "sim"            |
| 10  | `nao`        | "não"            |
| 11  | `cancela`    | "cancela"        |

### Formato obrigatório

- **Sample rate:** 16 000 Hz
- **Bit depth:** 16-bit PCM
- **Canais:** mono
- **Extensão:** `.wav`

### Estrutura no SD

```
/sdcard/kws/
  dorme_0.wav      dorme_1.wav      dorme_2.wav      dorme_3.wav      dorme_4.wav
  acorda_0.wav     acorda_1.wav     ...
  silencio_0.wav   ...
  privado_0.wav    ...
  que_horas_0.wav  ...
  como_voce_0.wav  ...
  olhe_para_mim_0.wav ...
  vol_alto_0.wav   ...
  vol_baixo_0.wav  ...
  sim_0.wav        ...
  nao_0.wav        ...
  cancela_0.wav    ...  cancela_4.wav
```

Total: **60 arquivos**.

### ⚠️ Use sua própria voz para os templates

**Não use TTS (ElevenLabs, Google, etc.) para os templates de keyword.**

O DTW compara a sua voz falando o comando contra os templates gravados.
Se os templates foram gerados por voz sintética e você fala com voz natural,
as características acústicas são diferentes e a acurácia cai bastante.

| Fonte dos templates   | Acurácia esperada |
| --------------------- | ----------------- |
| Sua voz (recomendado) | ⭐⭐⭐⭐⭐        |
| TTS + sua voz         | ⭐⭐              |

As frases são muito curtas ("sim", "não", "dorme") — gravar 60 arquivos leva ~15 minutos.

### Como gravar (opção A — PC com Audacity)

1. Abrir Audacity → Project Rate = **16000 Hz** (canto inferior esquerdo).
2. Gravar cada fala (~0.5–1s por comando, sem silêncio longo nas pontas).
3. Exportar: **File → Export → Export as WAV → PCM 16-bit Signed**.
4. Salvar com o nome exato (ex: `dorme_0.wav`).
5. Repetir 5 vezes por keyword (falar naturalmente, não roboticamente).
6. Copiar todos para `/sdcard/kws/` via leitor de cartão.

### Como gravar (opção B — direto pelo ESP32)

Adicionar um modo de gravação temporário que:

1. Aguarda wake word.
2. Captura 2s via `audio_capture_read`.
3. Salva no SD como WAV usando `sd_driver`.
4. Nomeia automaticamente (`{kw}_{n}.wav`) via serial.

---

## Passo 2 — Verificar carregamento dos templates

Após reiniciar com o SD inserido, o log deve mostrar:

```
I kws: tpl[0] kw=dorme   i=0 frames=XX
I kws: tpl[1] kw=dorme   i=1 frames=XX
...
I kws: ok — 60/60 templates, PSRAM ~XXX KB
I intent: ok — Core1 P10 capture=96KB
```

Se aparecer `template ausente`, verificar nome do arquivo e formato.

---

## Passo 3 — Teste de acurácia (critério de pronto)

### Protocolo

Para cada keyword:

1. Dizer o wake word ("Hey Jarvis" ou o configurado).
2. Após o LED vermelho acender, falar o comando.
3. Observar o log:
   ```
   I intent: captura concluída — XXXXX amostras
   I intent: intent=1 conf=85% kw=0(dorme)
   ```
4. Registrar: acerto / erro / INTENT_UNKNOWN indevido.

### Planilha de registro

| Keyword   | T1  | T2  | T3  | T4  | T5  | T6  | T7  | T8  | T9  | T10 | Acertos | %   |
| --------- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | ------- | --- |
| dorme     |     |     |     |     |     |     |     |     |     |     |         |     |
| acorda    |     |     |     |     |     |     |     |     |     |     |         |     |
| silencio  |     |     |     |     |     |     |     |     |     |     |         |     |
| privado   |     |     |     |     |     |     |     |     |     |     |         |     |
| que_horas |     |     |     |     |     |     |     |     |     |     |         |     |
| como_voce |     |     |     |     |     |     |     |     |     |     |         |     |
| me_olha   |     |     |     |     |     |     |     |     |     |     |         |     |
| vol_alto  |     |     |     |     |     |     |     |     |     |     |         |     |
| vol_baixo |     |     |     |     |     |     |     |     |     |     |         |     |
| sim       |     |     |     |     |     |     |     |     |     |     |         |     |
| nao       |     |     |     |     |     |     |     |     |     |     |         |     |
| cancela   |     |     |     |     |     |     |     |     |     |     |         |     |

**Meta:** ≥ 8/10 (80%) em cada keyword.

### Teste de falso positivo

Dizer 10 palavras aleatórias (ex: "mesa", "cadeira", "lua", ...) após o wake word.
Esperado: todas retornam `intent=0` (INTENT_UNKNOWN).

### Teste de latência

Observar no log os timestamps:

```
I wake_word: WAKE WORD! ww_index=0          ← t0
I intent:    intent=X conf=XX% kw=X(...)    ← t1
```

**Meta:** `t1 - t0 ≤ 3500ms` (3s captura + 500ms processamento).

---

## Passo 4 — Ajuste de threshold (se necessário)

Arquivo: `src/services/keyword_spotter/keyword_spotter.c`

```c
#define KWS_MATCH_THRESHOLD   3.5f   /* linha ~40 */
```

| Problema observado                          | Ação                                                                     |
| ------------------------------------------- | ------------------------------------------------------------------------ |
| Muitos INTENT_UNKNOWN (baixa sensibilidade) | Aumentar threshold (ex: `4.5f`)                                          |
| Match errado frequente (falso positivo)     | Diminuir threshold (ex: `2.5f`)                                          |
| Keyword específica com baixa acurácia       | Regravar templates dessa keyword com mais variação                       |
| Latência > 500ms                            | Reduzir `KWS_MAX_FRAMES` (ex: `80`) e `KWS_MAX_QUERY_FRAMES` (ex: `200`) |

---

## Critérios de pronto — checklist final

- [ ] 60 templates carregados sem erro no boot
- [ ] 12 keywords × 10 tentativas: todas com ≥ 80% de acurácia
- [ ] Palavras aleatórias → sempre INTENT_UNKNOWN
- [ ] Latência total ≤ 3.5s (3s captura + 500ms DTW)
- [ ] EVT_INTENT_DETECTED chegando a subscribers (verificar no log)

---

## Próxima etapa: E32 — TTS Pré-gravado + DialogueStateService

### Arquivos de resposta (E32)

Para E32 você pode usar **TTS sintético (ElevenLabs)** — sem problema, pois o robô
apenas _reproduz_ as respostas, não precisa reconhecê-las.

**Plano gratuito ElevenLabs:** 10.000 chars/mês — suficiente para todos os 41 arquivos.

**Fluxo:**

1. Gerar cada frase em elevenlabs.io (escolha uma voz em PT-BR)
2. Baixar como MP3
3. Converter para WAV 16kHz mono 16-bit:
   ```bash
   ffmpeg -i arquivo.mp3 -ar 16000 -ac 1 -sample_fmt s16 arquivo.wav
   ```
   Ou converter todos de uma vez:
   ```bash
   for f in *.mp3; do
     ffmpeg -i "$f" -ar 16000 -ac 1 -sample_fmt s16 "${f%.mp3}.wav"
   done
   ```

**37 arquivos necessários** em `/sdcard/tts/` (aceita `.wav` ou `.pcm`):

| Arquivo                        | Frase sugerida                              |
| ------------------------------ | ------------------------------------------- |
| `nao_entendi.wav`              | "Não entendi, pode repetir?"                |
| `pode_repetir.wav`             | "Não ouvi nada, pode repetir?"              |
| `ok.wav`                       | "Ok"                                        |
| `vou_dormir.wav`               | "Tá bom, vou dormir"                        |
| `estou_acordado.wav`           | "Olá! Estou acordado"                       |
| `silencio.wav`                 | "Ok, fico quieto"                           |
| `estou_bem.wav`                | "Estou bem, obrigado"                       |
| `te_vejo.wav`                  | "Aqui estou"                                |
| `volume_alto.wav`              | "Volume aumentado"                          |
| `volume_baixo.wav`             | "Volume reduzido"                           |
| `sim.wav`                      | "Sim"                                       |
| `nao.wav`                      | "Não"                                       |
| `cancelando.wav`               | "Cancelado"                                 |
| `hora_0.wav` … `hora_23.wav`   | "São zero horas" … "São vinte e três horas" |

> Os 24 arquivos de hora são opcionais — se ausentes, "que horas são" toca `nao_entendi.wav`.

### Resumo: qual voz usar para cada coisa

| O quê                    | Fonte recomendada                    | Motivo                            |
| ------------------------ | ------------------------------------ | --------------------------------- |
| Templates E31 (comandos) | **Sua voz**                          | Reconhecimento requer a mesma voz |
| Respostas E32 (TTS)      | \*\*                                 |
| \*\*                     | Só reprodução, qualquer voz funciona |

### DialogueStateService — FSM

```
IDLE → LISTENING → PROCESSING → SPEAKING → IDLE
```

- Timeouts: LISTENING 5s, PROCESSING 2s
- Chamar `wake_word_suppress_ms()` ao entrar em SPEAKING
- Publicar `EVT_DIALOGUE_STATE_CHANGED` em cada transição
