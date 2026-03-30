# E26 - TouchService e Reações Básicas

- Status: 🔲 Não iniciada
- Complexidade: Baixa
- Grupo: Interação
- HW Real: SIM
- Prioridade: P2
- Risco: BAIXO
- Depende de: 

## TouchService e Reações Básicas

Complexidade: Baixa
Grupo: Interação
HW Real: SIM
ID: E26
Prioridade: P2
Risco: BAIXO
Status: 🔲 Não iniciada

## Objetivo

TouchService com debounce 50ms, zonas calibradas, baseline em NVS. EVT_TOUCH_DETECTED chegando ao BehaviorEngine. Latência fim-a-fim < 150ms.

## Critérios de pronto

- Toque em ZONE_TOP: expressão SURPRISED em < 150ms
- 0 falsos positivos em 10 minutos sem toque
- Calibração em NVS: reboot + toque imediato funciona sem recalibrar

## Testes mínimos

- 10 minutos sem toque: 0 falsos positivos
- 50 toques em 4 zonas: medir latência com log timestamp
- Reboot + toque imediato: verificar que calibração NVS é usada

---

## ▶ Prompt Principal

```
Contexto: touch_driver (E09), EventBus (E12), state_vector (E21). E26 — TouchService.
Tarefa: touch_service.h + touch_service.c.
touch_service_init(): ler threshold por zona do NVS (ou chamar touch_service_calibrate() se ausente).
touch_service_calibrate(): 100 amostras por zona, threshold = média × 0.75, salvar em NVS.
Loop: a cada 20ms (P7 task), verificar is_touched() por zona; se nova detecção + debounce 50ms: publicar EVT_TOUCH_DETECTED{zone_id, intensity=(raw-threshold)/threshold, duration_ms}.
Reação básica: EVT_TOUCH_DETECTED → arousal = min(1.0, arousal + 0.3); emotion_mapper_apply(SURPRISED, 200ms).
Saída: touch_service.h/.c + EVT_TOUCH_DETECTED definido em event_types.h.
```

## ◎ Prompt de Revisão

```
TouchService da E26.
Verificar: (1) baseline lido do NVS (não hardcoded)? (2) debounce implementado (50ms)? (3) EVT_TOUCH_DETECTED tem todos os campos? (4) latência fim-a-fim ≤ 150ms?
Listar problemas.
```

## ✎ Prompt de Correção

```
TouchService com problema: [sintoma]
Contexto: E26.
Causa + fix.
```

## → Prompt de Continuidade

```
E26 concluída. Touch detectando 4 zonas, reação facial em < 150ms.
Próxima: E27 (IMUService e eventos de orientação).
Mostre como implementar IMUService que detecta shake, tilt e queda, incluindo integração com MotionSafetyService para parada de servo em queda.
```


