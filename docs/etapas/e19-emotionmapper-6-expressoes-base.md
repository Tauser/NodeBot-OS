# E19 - EmotionMapper - 6 Expressoes Base

- Status: ✅ Criterios atendidos
- Complexidade: Baixa
- Grupo: Face
- HW Real: SIM
- Notas: E19 mapeia estados emocionais para faces base reconheciveis. A etapa nao define gaze runtime nem efeitos internos; ela escolhe o `shape base` que sera modulado pelas camadas dinamicas posteriores.
- Prioridade: P1
- Risco: BAIXO
- Depende de: E17 + E18

## Objetivo

Traduzir 6 estados emocionais base para conjuntos de `face_params_t` reutilizaveis.

Interface alvo:

```c
emotion_mapper_apply(EMOTION_HAPPY, 300);
```

## Regra da etapa

- E19 escolhe a expressao base.
- E17 continua sendo responsavel pelo shape e pela interpolacao.
- E18 continua sendo responsavel por blink, drift e micro-movimentos.
- `gaze` social, tracking e saccades nao fazem parte do mapeamento base desta etapa.

## Expressoes base

| Emocao    | Base usada       | Leitura principal                   |
|-----------|------------------|-------------------------------------|
| NEUTRAL   | `FACE_NEUTRAL`   | estado padrao, sem tensao           |
| HAPPY     | `FACE_HAPPY`     | positiva, acolhedora                |
| SAD       | `FACE_SAD_DOWN`  | baixa energia, abatimento           |
| FOCUSED   | `FACE_FOCUSED`   | atencao, concentracao, thinking     |
| ANGRY     | `FACE_ANGRY`     | tensao, irritacao, alerta agressivo |
| SURPRISED | `FACE_SURPRISED` | abertura maxima, reacao imediata    |

Observacoes:

- `FOCUSED` substitui a ideia antiga de "thinking" com base em geometria, nao em gaze embutido.
- O mapper deve limpar `gaze_x` e `gaze_y` da face base, para que o olhar continue sendo controlado pelo runtime.
- As expressoes vindas do mapper devem usar prioridade de `MOOD`, deixando reacoes de maior prioridade para camadas superiores.

## Criterios de pronto

- 3 pessoas sem briefing identificam corretamente 5 das 6 expressoes
- Transicao entre qualquer par de expressoes sem salto visual
- `emotion_mapper_apply()` aceita override de `transition_ms`
- A expressao base continua reconhecivel mesmo com blink e micro-movimentos ativos

## Testes minimos

- Ciclar 6 expressoes e validar leitura visual
- Transicao `NEUTRAL -> HAPPY -> NEUTRAL`
- Transicao `NEUTRAL -> SURPRISED -> NEUTRAL`
- Verificar que `emotion_mapper_get()` retorna faces com `gaze_x = 0` e `gaze_y = 0`
- Verificar que a prioridade final do mapper e `FACE_PRIORITY_MOOD`

## Prompt principal

```text
Contexto: face_params_t definida (E17), runtime facial da E18 ativo. E19 - EmotionMapper.
Tarefa: emotion_mapper.h + emotion_mapper.c.
Definir 6 expressoes base: NEUTRAL, HAPPY, SAD, FOCUSED, ANGRY, SURPRISED.
Interface minima:
- emotion_mapper_get(emotion_t e, face_params_t *out)
- emotion_mapper_apply(emotion_t e, uint16_t transition_ms)
Regra:
- mapper entrega somente a face base
- limpar gaze_x/gaze_y antes de aplicar
- prioridade final = FACE_PRIORITY_MOOD
Saida: emotion_mapper.h/.c integrados ao face_engine.
```

## Prompt de revisao

```text
EmotionMapper da E19.
Verificar: (1) 6 expressoes visualmente distintas? (2) FOCUSED nao depende de gaze embutido? (3) transicao suave entre qualquer par? (4) mapper limpa gaze_x/gaze_y? (5) prioridade final = MOOD?
Listar problemas.
```

## Prompt de correcao

```text
Expressao [NOME] nao esta convincente.
Contexto: E19, emotion_mapper e face procedural.
Ajustar parametros da expressao base sem empurrar a leitura para o gaze runtime.
Saida: novos valores com justificativa.
```

## Continuidade

```text
E19 concluida. 6 expressoes base reconheciveis entregues pelo EmotionMapper.
Proxima: E20 (Gaze loop e saccades).
Implementar gaze social e saccades realistas por cima da expressao base, sem redefinir o shape vindo do mapper.
```
