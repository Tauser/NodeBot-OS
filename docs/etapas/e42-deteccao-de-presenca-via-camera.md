# E42 - Detecção de Presença via Câmera

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Câmera/Cloud
- HW Real: SIM
- Notas: Depende de E36 (CameraService) concluída
- Prioridade: P3
- Risco: MÉDIO
- Depende de: E36 (CameraService)

## Detecção de Presença via Câmera

Complexidade: Alta
Depende de: E36 (CameraService)
Grupo: Câmera/Cloud
HW Real: SIM
ID: E42
Notas: Depende de E36 (CameraService) concluída
Prioridade: P3
Risco: MÉDIO
Status: 🔲 Não iniciada

> 📍 **Etapa E42** · Grupo: Câmera/Cloud · Prioridade: P3 · Depende de: **E36** (CameraService)
> 

## Objetivo

Detecção de presença humana e rosto via câmera com modelo leve de ML (ESP-WHO ou heurística HOG), publicando posição normalizada do rosto para consumo pelo BehaviorEngine e pelo futuro FaceTracker (E43).

A E36 entrega o CameraService (infraestrutura de captura). A E42 constrói **o que fazer com o frame** — passar de "tem movimento" para "tem pessoa" e "rosto está em X,Y".

---

## Por que agora

Sem detecção de presença real, o BehaviorEngine não sabe se alguém está na frente do robô. Com isso: gaze vai para a posição do rosto, arousal sobe ao detectar pessoa, e o FaceTracker (E43) tem coordenadas para mover o servo.

---

## O que implementar

- `presence_detector.h/.c`: recebe `EVT_CAMERA_FRAME`, roda detector, publica `EVT_PERSON_DETECTED`
- Detector de dois níveis:
    - **Nível 1 — frame diff** (já na E36): filtro rápido, descarta frames sem movimento
    - **Nível 2 — detecção de rosto**: só roda quando frame diff > threshold
- Modelo recomendado: **ESP-WHO face detection** (binário de ~200KB, roda em Core 0)
- Alternativa leve: **Viola-Jones HOG** implementado manualmente (~40KB RAM)
- Output: `face_detection_result_t { bool detected; float x; float y; float confidence; uint16_t w; uint16_t h; }`
- Publicar `EVT_PERSON_DETECTED { detected, face_x, face_y, confidence }` — coordenadas normalizadas 0.0–1.0
- Publicar apenas quando estado muda (presente→ausente ou vice-versa) — não a cada frame
- `presence_detector_get_last_result()` para polling síncrono pelo BehaviorEngine

## O que NÃO entra

- Movimento de servo para rastrear o rosto (vem na E43)
- Reconhecimento de pessoa específica (hardware insuficiente)
- Streaming contínuo de frames
- Detecção de emoção por câmera

## Critérios de pronto

- `EVT_PERSON_DETECTED(detected=true)` ao entrar no campo de visão em ≤ 2s
- `EVT_PERSON_DETECTED(detected=false)` ao sair em ≤ 3s
- Coordenadas `face_x, face_y` dentro de ±10% da posição real (validar visualmente)
- CPU Core 0 com detecção ativa ≤ 25% (pico durante janela de detecção)
- FPS da face Core 1 não cai abaixo de 15 durante detecção

## Testes mínimos

- Entrar no campo 10×: `EVT_PERSON_DETECTED(detected=true)` em ≤ 2s todas as vezes
- Sair do campo 10×: `EVT_PERSON_DETECTED(detected=false)` em ≤ 3s
- Mover o rosto para os 4 quadrantes: verificar que `face_x/face_y` refletem a posição
- CPU: medir Core 0 com detecção ativa vs. idle
- FPS: verificar Core 1 durante janela de detecção

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, OV2640, CameraService da E36 funcionando (EVT_CAMERA_FRAME disponível), Core 0.
E42 — Detecção de presença e rosto.

Tarefa: presence_detector.h + presence_detector.c.

Arquitetura de dois níveis:
  Nível 1: frame_diff_score = camera_service_get_frame_diff(prev, curr).
    Se diff_score < MOTION_THRESHOLD: descartar frame, não rodar detector.
  Nível 2: se diff_score >= threshold: rodar face_detector_run(frame) → face_detection_result_t.

Escolher implementação do detector (em ordem de preferência):
  A) ESP-WHO face_detection: incluir componente esp-who, chamar face_detect_run().
  B) HOG simples: janela deslizante 24×24, gradientes, threshold de energia.

Output struct:
  typedef struct {
    bool    detected;
    float   x;          // centro do rosto, 0.0 = esquerda, 1.0 = direita
    float   y;          // 0.0 = topo, 1.0 = baixo
    float   confidence; // 0.0–1.0
    uint16_t w, h;      // pixels do bounding box
  } face_detection_result_t;

Publicar EVT_PERSON_DETECTED{detected, face_x, face_y, confidence} via EventBus.
Regra de publicação: apenas quando estado muda (presente→ausente ou vice-versa).
Rodar assincronamente no Core 0 como sub-módulo da CameraCaptureTask.

Saída: presence_detector.h/.c + como integrar no camera_service.c.
```

## ◎ Prompt de Revisão

```
Presence detector da E42.
Verificar: (1) dois níveis de detecção (diff primeiro, detector só se necessário)? (2) publica apenas quando estado muda (não a cada frame)? (3) coordenadas normalizadas 0–1? (4) roda no Core 0 (não Core 1)? (5) CPU Core 0 ≤ 25% durante detecção?
Listar problemas.
```

## ✎ Prompt de Correção

```
Presence detector com problema: [sintoma — ex: face_x sempre 0.5 independente da posição]
Contexto: E42, ESP32-S3, OV2640.
Verificar: mapeamento de coordenadas do bounding box para 0–1, resolução do frame de entrada.
Causa + fix.
```

## → Prompt de Continuidade

```
E42 concluída. Detecção de presença e posição do rosto funcionando.
Coordenadas face_x, face_y disponíveis via EVT_PERSON_DETECTED.
Próxima: E43 (Face Tracking com servo de pescoço).
Mostre como usar face_x e face_y para calcular o ângulo alvo do servo e implementar o loop de tracking PID simples.
```


