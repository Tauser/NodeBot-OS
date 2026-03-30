# E18 - Micro-movimentos e Blink Automatico

- Status: ✅ Criterios atendidos
- Complexidade: Baixa
- Grupo: Face
- HW Real: SIM
- Prioridade: P1
- Risco: BAIXO
- Depende de: E17

## Objetivo

Adicionar vida sobre a face base definida na E17.

Nesta etapa, a expressao salva nao muda de shape. O que acontece e a aplicacao de modulacoes de runtime por cima da face base:

- blink automatico
- micro-movimentos sutis
- idle drift
- pequenas variacoes de olhar

E18 existe para que a face nunca pareca completamente estatica, sem redefinir a expressao base escolhida.

## Escopo da etapa

- Aplicar micro-movimentos periodicos sobre o gaze/runtime do frame.
- Implementar blink automatico com timing natural.
- Permitir supressao temporaria de blink em eventos futuros como saccades ou tracking.
- Manter CPU adicional baixa.
- Preservar a leitura da expressao base da E17.

## Modelo conceitual

Separacao esperada entre E17 e E18:

- E17: shape base da expressao.
- E18: modulacao temporal do runtime facial.

Formula mental:

```text
face_final = face_base + runtime_gaze + micro_movimento + blink
```

Observacao:

- `squint` pode continuar vindo da expressao base.
- `gaze` aqui deve ser entendido como alvo/runtime do olhar.
- micro-movimentos nao substituem o gaze; eles apenas perturbam levemente esse alvo.

## Criterios de pronto

- 30s de observacao: face nunca esta completamente parada
- Blink: taxa entre 12 e 20 por minuto
- Observador externo descreve como "respira" ou "esta pensando" em 10s de observacao
- A expressao base continua reconhecivel durante blink e drift

## Testes minimos

- Observar por 30s sem interacao: verificar que nunca para
- Contar blinks em 1 minuto: deve ser entre 12 e 20
- Verificar que blink nao deforma permanentemente a expressao base
- Validar que micro-movimentos oscilam ao redor do gaze/runtime atual
- CPU extra: `vTaskGetRunTimeStats()` antes e depois, diferenca < 2%

## Prompt principal

```text
Contexto: FaceEngine com face base da E17. E18 - micro-movimentos e blink automatico.
Tarefa A: adicionar micro-movimentos ao render loop em face_engine.cpp.
Implementar runtime_gaze_x = gaze_base_x + drift_x + micro_x;
Implementar runtime_gaze_y = gaze_base_y + drift_y + micro_y;
Sugestao inicial:
- drift_x = sin(2pi * 0.28Hz * t_sec) * 1.5px
- drift_y = sin(2pi * 0.22Hz * t_sec + 1.2rad) * 1.0px
- micro_x e micro_y com pequenas perturbacoes aleatorias suaves
Tarefa B: blink_controller.h + blink_controller.c/.cpp.
Logica: gerar proximo blink em rand(2500, 5000)ms. Sequencia: fechar eyelid em 80ms -> abrir em 120ms. Taxa base 15/min, ajustavel por energy (0-1).
Gancho: blink_suppress() para inibir blink durante saccade ou tracking futuro.
Regra: blink e micro-movimentos modulam a face base, mas nao redefinem o shape salvo da expressao.
Saida: atualizacao do face_engine + blink_controller + integracao no render loop.
```

## Prompt de revisao

```text
Micro-movimentos e blink da E18.
Verificar: (1) amplitude <= +-2px? (2) fase de drift_x diferente de drift_y? (3) blink tem duracao realista (80ms fechar, 120ms abrir)? (4) blink_suppress() existe? (5) runtime modula a face base sem destruir a leitura da expressao?
Listar problemas.
```

## Prompt de correcao

```text
Micro-movimentos parecem mecanicos ou blink parece robotico.
Contexto: E18, face_engine e blink_controller.
Ajustar parametros para naturalidade sem exagerar a amplitude nem apagar a expressao base.
Saida: valores ajustados + justificativa.
```

## Continuidade

```text
E18 concluida. Face base da E17 continua estavel, com blink automatico e micro-movimentos em runtime.
Proxima: E19 (EmotionMapper - expressoes base mapeadas para face_params_t).
Mostre como definir os estados emocionais base sem misturar shape da expressao com comportamento dinamico de runtime.
```
