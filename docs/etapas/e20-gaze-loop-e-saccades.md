# E20 - Gaze Loop e Saccades

- Status: ✅ Criterios atendidos
- Complexidade: Media
- Grupo: Gaze+Idle
- HW Real: SIM
- Prioridade: P1
- Risco: BAIXO
- Depende de: E17 + E18 + E19

## Objetivo

Implementar `GazeService` como a camada principal de direcao do olhar em runtime.

A E20 controla:

- alvo atual de gaze
- saccades com overshoot
- retorno suave ao alvo final
- idle gaze nao repetitivo

A etapa nao redefine a expressao base da E19. Ela modula apenas o olhar, por cima da face base e convivendo com blink e micro-movimentos da E18.

## Regras da etapa

- `gaze` e runtime, nao shape base.
- O olhar deve ser controlado por um servico proprio.
- Saccades devem ter overshoot perceptivel.
- Blink automatico pode ser temporariamente suprimido durante a saccade.
- Idle gaze deve evitar padrao mecanico ou repetitivo.

## Comportamento esperado

### Saccade

1. Recebe um alvo `(x, y)`.
2. Calcula overshoot:

```text
overshoot = target + (target - current) * 0.12
```

3. Vai para o overshoot com ease-out.
4. Retorna ao alvo final em 80 ms.
5. Libera blink automatico ao concluir.

### Idle gaze

- A cada intervalo aleatorio `rand(2000, 5000) ms`
- Gera:
  - `x ~ N(0, 0.3)`
  - `y ~ N(0, 0.2)`
- Clamp em `+-0.8`

## Criterios de pronto

- 20 saccades observadas: >= 15 com overshoot perceptivel
- 2 minutos de idle: nenhum padrao de repeticao obvio
- Blink nao interfere visualmente em saccades
- CPU Core 1 com Face + Gaze <= 35%

## Testes minimos

- 20 saccades: contar quantas tem overshoot visivel
- 2 minutos de idle: observar ausencia de padrao mecanico
- Validar que `EVT_GAZE_UPDATE` e publicado via EventBus
- Validar que `face_engine_set_gaze()` recebe o estado atual do `GazeService`
- Validar que blink automatico e suprimido enquanto a saccade esta ativa

## Prompt principal

```text
Contexto: ESP32-S3, FaceEngine com face base pronta, runtime de blink e micro-movimentos ativo. E20 - GazeService.
Tarefa: implementar gaze_service.h + gaze_service.c.
API:
- gaze_service_init()
- gaze_service_set_target(float x, float y, uint16_t duration_ms)
- gaze_service_get(float *x, float *y)
Implementar:
1. overshoot = target + (target - current) * 0.12
2. ida ao overshoot em duration_ms * 0.7 com ease-out
3. retorno ao target em 80 ms
4. idle gaze com gaussian e intervalo aleatorio
5. publicacao de EVT_GAZE_UPDATE
6. supressao temporaria de blink durante saccade
Regra: E20 controla o olhar em runtime, sem redefinir o shape base da expressao.
Saida: gaze_service.h/.c integrados ao face_engine e event_bus.
```

## Prompt de revisao

```text
GazeService da E20.
Verificar: (1) overshoot implementado? (2) idle usa distribuicao gaussiana? (3) intervalo de idle e aleatorio? (4) EVT_GAZE_UPDATE e publicado? (5) blink e suprimido durante saccade? (6) gaze nao redefine a expressao base?
Listar problemas.
```

## Prompt de correcao

```text
Saccades parecem mecanicas ou sem overshoot visivel.
Contexto: E20, GazeService.
Ajustar parametros de overshoot, duracao e curva de velocidade sem empurrar comportamento para a face base.
Saida: valores corrigidos + justificativa.
```

## Continuidade

```text
E20 concluida. Saccades com overshoot e idle gaze variado aplicados em runtime.
Proxima: E21 (vetor de estado interno - energy, mood, arousal).
Mostre como definir state_vector_t e como ligar suas dimensoes a blink, emocao base e comportamento futuro.
```
