# E43 - Face Tracking com Servo de Pescoço

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Câmera/Cloud
- HW Real: SIM
- Notas: Depende de E42 (presença) e E23 (MotionSafety). Servo de pescoço = hardware adicional ao baseline.
- Prioridade: P3
- Risco: ALTO
- Depende de: E42 + E23 + E34

## Face Tracking com Servo de Pescoço

Complexidade: Alta
Depende de: E42 + E23 + E34
Grupo: Câmera/Cloud
HW Real: SIM
ID: E43
Notas: Depende de E42 (presença) e E23 (MotionSafety). Servo de pescoço = hardware adicional ao baseline.
Prioridade: P3
Risco: ALTO
Status: 🔲 Não iniciada

> 📍 **Etapa E43** · Grupo: Câmera/Cloud · Prioridade: P3 · Depende de: **E42** (Detecção de Presença) + **E23** (MotionSafety) + **E34** (GestureService)
> 

## Objetivo

Mover o servo de pescoço para rastrear o rosto detectado pela câmera. O robô olha para a pessoa — combinando `face_x` da E42 com controle PID do servo para movimento suave e seguro.

---

## Por que agora

A E42 sabe **onde** o rosto está. A E43 usa essa informação para **mover fisicamente** o robô em direção ao rosto. É o que transforma presença passiva em atenção física percebida.

---

## Nota de hardware

> O baseline do projeto tem **2× SCS0009** para expressão (pan/tilt da cabeça). Face tracking assume que um deles (ou um terceiro servo adicional) cobre o eixo de pan (horizontal). Confirmar qual servo ID é o pescoço antes de implementar.
> 

| Eixo | Servo ID | Faixa | Descrição |
| --- | --- | --- | --- |
| Pan (horizontal) | [confirmar] | 256–768 (center=512) | Rotação esquerda/direita |
| Tilt (vertical) | [confirmar ou N/A] | 400–624 (center=512) | Inclinação cima/baixo |

---

## O que implementar

- `face_tracker.h/.c`: assina `EVT_PERSON_DETECTED`, calcula erro, roda PID, envia para servo via MotionSafety
- **PID simples** (P + D são suficientes para este caso):
    - `error_x = face_x - 0.5` (0.5 = centro do frame)
    - `servo_delta = Kp * error_x + Kd * (error_x - last_error_x)`
    - `servo_target = servo_current + servo_delta * SERVO_SCALE`
- **Dead zone**: não mover se `|error_x| < 0.05` — evita jitter quando rosto está centralizado
- **Velocity limit**: máx `±30 unidades` por ciclo (movimento suave, não brusco)
- **Integração obrigatória com MotionSafetyService**: cada movimento passa por `motion_safety_check()`
- **Cooldown de tracking**: não rastrear nos primeiros 500ms após wake word (robô está falando)
- **Fallback**: se nenhum rosto detectado por > 5s → retornar servo para posição central (512)
- Publicar `EVT_FACE_TRACKER_UPDATE { target_angle, current_angle, tracking_active }`

## O que NÃO entra

- Tracking de múltiplos rostos (rastrear o maior/mais central)
- Predição de movimento (Kalman filter — complexidade desnecessária agora)
- Tilt vertical (implementar apenas pan horizontal inicialmente)
- Integração com GazeService do FaceEngine (coordenar é opcional — gaze virtual pode seguir independentemente)

## Critérios de pronto

- Rosto centralizado no frame em ≤ 2s após aparecer fora do centro
- Sem oscilação (jitter) quando rosto está parado no centro
- `motion_safety_check()` retorna BLOCKED → tracking para imediatamente
- Sem movimento de servo durante conversação ativa (cooldown funcionando)
- Retorno ao centro após 5s sem rosto detectado

## Testes mínimos

- Mover rosto lentamente da esquerda para direita: servo segue suavemente
- Parar rosto no centro: servo para sem oscilar
- Bloquear servo mecanicamente durante tracking: MotionSafety detecta, tracking para
- Falar para o robô (wake word): verificar que servo não se move durante resposta
- Sair do campo por 6s: verificar retorno ao centro

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, SCS0009 via FE-TTLinker (E04), MotionSafetyService (E23), EVT_PERSON_DETECTED disponível (E42).
E43 — Face Tracker com servo de pescoço.

Tarefa: face_tracker.h + face_tracker.c.

FaceTrackerTask: Core 1, P9 (abaixo de Behavior P12), vTaskDelayUntil 50ms (20Hz).

Lógica a cada tick:
  1. Ler last_face_result = presence_detector_get_last_result()
  2. Se !detected OU cooldown_active: incrementar absent_counter
     - Se absent_counter > 100 (5s): face_tracker_return_center()
     - return
  3. Calcular erro: error_x = last_face_result.face_x - 0.5
  4. Dead zone: se |error_x| < 0.05: return
  5. PID:
     float delta = Kp * error_x + Kd * (error_x - last_error_x)
     last_error_x = error_x
  6. Calcular target: target = current_pos + (int16_t)(delta * SERVO_SCALE)
     target = clamp(target, PAN_MIN, PAN_MAX)  // ex: 256–768
  7. Limitar velocidade: se |target - current_pos| > MAX_STEP: target = current_pos ± MAX_STEP
  8. Verificar segurança: if(!motion_safety_check(PAN_SERVO_ID, target)) return
  9. scs0009_set_position(PAN_SERVO_ID, target, TRACKING_SPEED)
  10. current_pos = target

Cooldown: assinar EVT_DIALOGUE_STATE_CHANGED; se estado != IDLE: cooldown_active = true por 500ms após voltar a IDLE.

Parâmetros iniciais (ajustar na bancada):
  Kp = 80.0f, Kd = 20.0f, SERVO_SCALE = 100.0f
  MAX_STEP = 30, PAN_MIN = 256, PAN_MAX = 768
  PAN_SERVO_ID = [confirmar ID do servo de pescoço]

Saída: face_tracker.h/.c.
```

## ◎ Prompt de Revisão

```
Face Tracker da E43.
Verificar: (1) cada set_position passa por motion_safety_check()? (2) dead zone implementada (sem jitter no centro)? (3) velocity limit implementado (MAX_STEP)? (4) cooldown durante conversação? (5) retorno ao centro após ausência? (6) task em P9 (abaixo de Behavior P12)?
Listar problemas com severidade.
```

## ✎ Prompt de Correção

```
Face tracker com problema: [sintoma — ex: servo oscila quando rosto está centralizado / tracking muito lento]
Contexto: E43, PID simples, SCS0009.
Ajustar parâmetros: Kp muito alto = oscilação; Kp muito baixo = lento. Kd amortece oscilação.
Saída: valores ajustados de Kp, Kd, MAX_STEP + justificativa.
```

## → Prompt de Continuidade

```
E43 concluída. Face tracking com servo de pescoço funcionando, movimento suave, safety integrado.
O robô fisicamente olha para a pessoa.
Próximas etapas disponíveis: E38 (Observabilidade) ou retornar ao fluxo de produto.
Sugestão: integrar face_tracker com GazeService — quando tracker ativo, gaze virtual do FaceEngine segue face_x também.
```


