# E27 - IMUService e Eventos de Orientação

- Status: 🔲 Não iniciada
- Complexidade: Baixa
- Grupo: Interação
- HW Real: SIM
- Prioridade: P2
- Risco: BAIXO
- Depende de: 

## IMUService e Eventos de Orientação

Complexidade: Baixa
Grupo: Interação
HW Real: SIM
ID: E27
Prioridade: P2
Risco: BAIXO
Status: 🔲 Não iniciada

## Objetivo

IMUService detectando SHAKE, TILT e FALL. Queda para servos imediatamente via MotionSafety. Contexto de orientação disponível para BehaviorEngine.

## Critérios de pronto

- Agitar o robô: EVT_MOTION_DETECTED(SHAKE) publicado
- Inclinar 45°: EVT_MOTION_DETECTED(TILT) após 2s
- Impacto leve na mesa: FALL detectado + servo parado

## Testes mínimos

- Agitar: verificar EVT_MOTION_DETECTED(SHAKE)
- Inclinar 45°: verificar TILT após 2s
- Impacto leve na mesa: verificar FALL + servo parado

---

## ▶ Prompt Principal

```
Contexto: imu_driver (E08), EventBus (E12), motion_safety (E23). E27 — IMUService.
Tarefa: imu_service.h + imu_service.c.
IMUTask: Core 1, P7, 50ms.
Detecções a cada tick:
  FALL: |accel| < 200mg (freefall) por >100ms → motion_safety_emergency_stop() + EVT_MOTION_DETECTED(FALL)
  SHAKE: variância de accel_xyz em janela de 500ms > 200mg² → EVT_MOTION_DETECTED(SHAKE, intensity)
  TILT: ângulo > 40° por >2s → EVT_MOTION_DETECTED(TILT, angle_deg)
Interface pública: imu_service_is_upright() → bool, imu_service_get_tilt_deg() → float.
Saída: imu_service.h/.c.
```

## ◎ Prompt de Revisão

```
IMUService da E27.
Verificar: (1) FALL chama motion_safety_emergency_stop() (não só publica evento)? (2) threshold de SHAKE não dispara com vibração normal de servo? (3) is_upright() correto em todas as orientações?
Listar problemas.
```

## ✎ Prompt de Correção

```
IMUService com problema: [sintoma]
Contexto: E27.
Causa + fix.
```

## → Prompt de Continuidade

```
E27 concluída. IMUService com SHAKE, TILT e FALL. Interação física COMPLETA.
Próxima: E28 (pipeline de captura de áudio e VAD).
Mostre como estruturar AudioCaptureTask no Core 0 com buffer circular e VAD por energia + ZCR + passa-banda.
```


