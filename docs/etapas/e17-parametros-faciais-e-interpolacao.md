# E17 - Parametros Faciais e Interpolacao

- Status: ✅ Criterios atendidos
- Complexidade: Media
- Grupo: Face
- HW Real: SIM
- Notas: E17 define o `shape base` da face. A expressividade principal vem da geometria procedural dos olhos, abertura, curvatura, arredondamento, cor e `squint` opcional. `gaze` pertence principalmente ao runtime e nao ao desenho-base da expressao.
- Prioridade: P1
- Risco: MEDIO
- Depende de: E16 + Design Sprint

## Objetivo

Definir `face_params_t`, interpolar estados com cubic ease-in-out e renderizar olhos proceduralmente usando `LGFX_Sprite`, tomando a face como um modelo unificado baseado no contorno dos olhos.

## Escopo da etapa

- Definir a geometria base dos olhos por lado.
- Permitir expressoes assimetricas com parametros independentes por olho.
- Interpolar transicoes entre expressoes sem salto visual.
- Renderizar os olhos proceduralmente com LovyanGFX.
- Permitir `squint` como refinamento visual da expressao base.
- Manter `gaze` suportado no pipeline, mas entendido como camada de runtime aplicada por cima da face base.

## Parametros base da face

Os campos centrais da E17 sao:

- `tl_l`, `tr_l`, `bl_l`, `br_l`
- `tl_r`, `tr_r`, `bl_r`, `br_r`
- `open_l`, `open_r`
- `y_l`, `y_r`
- `x_off`
- `rt_top`, `rb_bot`
- `cv_top`, `cv_bot`
- `color`
- `squint_l`, `squint_r`
- `transition_ms`
- `priority`

Observacao:

- `squint` pode compor a expressao base.
- `gaze_x` e `gaze_y` existem no pipeline e podem ser interpolados pelo engine, mas nao devem ser tratados como o nucleo do desenho-base da face.

## Renderizacao procedural com LovyanGFX

```cpp
void render_eye(lgfx::LGFX_Sprite *buf, const face_params_t *p, int cx, int cy) {
  int ew = 60;
  int eh = (int)(80.0f * p->open_l);
  if (eh < 2) eh = 2;

  draw_eye_shape(buf, cx, cy, ew, eh, p->tl_l, p->tr_l, p->bl_l, p->br_l,
                 p->rt_top, p->rb_bot, p->cv_top, p->cv_bot, p->color);

  int rise = (int)(p->squint_l * eh * 0.5f);
  if (rise > 0) {
    draw_squint_mask(buf, cx, cy, ew, eh, rise);
  }
}
```

Observacao:

- O renderer da E17 desenha a forma base do olho.
- Blink, micro-movimentos, idle drift e direcao do olhar sao modulacoes posteriores do runtime.

## Criterios de pronto

- 5 expressoes: pessoa nao tecnica identifica corretamente 4 de 5
- Transicao de 300ms sem salto visual
- CPU Core 1 <= 30% com interpolacao ativa

## Testes minimos

- Ciclar 5 expressoes e validar reconhecimento visual
- Transicao neutro -> surpreso -> neutro suave
- Validar assimetria entre olho esquerdo e direito
- Aplicar runtime de `gaze` por cima da face base sem quebrar a leitura da expressao

## Prompt principal

```text
Contexto: ESP32-S3, LovyanGFX, LGFX_Sprite *draw_buf como canvas de renderizacao (E16). E17 - parametros faciais.
Tarefa A: definir face_params_t com geometria procedural dos olhos, open_l/open_r, offsets de canto por olho, curvatura, arredondamento, cor, squint_l/squint_r, transition_ms e priority.
Tarefa B: face_engine_apply(face_params_t *target) interpola estado atual -> target em transition_ms usando cubic ease-in-out.
Tarefa C: render_face(lgfx::LGFX_Sprite *buf, face_params_t *p) usando deformacao procedural do olho e squint diagonal.
Observacao: `gaze` pode existir no pipeline do engine, mas nao deve ser tratado como centro da expressao base nesta etapa.
Clampar todos os parametros no range valido antes de renderizar.
Saida: face_params.h + atualizacao de face_engine.cpp + 5 expressoes de exemplo.
```

## Prompt de revisao

```text
Parametros faciais da E17 com LovyanGFX.
Verificar: (1) cubic ease-in-out? (2) clamp de todos os parametros? (3) renderizacao usa API do Sprite? (4) face_params_t tem priority? (5) a etapa esta centrada no shape base da face e nao em gaze runtime?
Listar problemas.
```

## Continuidade

```text
E17 concluida. Face base definida com geometria procedural, interpolacao suave e suporte a expressoes assimetricas.
Proxima: E18 (micro-movimentos e blink automatico).
Adicionar blink, idle drift e runtime gaze por cima da face base, sem redefinir o shape salvo da expressao.
```
