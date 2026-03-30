# E36 - CameraService — Captura Assíncrona

- Status: 🔲 Não iniciada
- Complexidade: Alta
- Grupo: Câmera/Cloud
- HW Real: SIM
- Prioridade: P3
- Risco: MÉDIO
- Depende de: 

## CameraService — Captura Assíncrona

Complexidade: Alta
Grupo: Câmera/Cloud
HW Real: SIM
ID: E36
Prioridade: P3
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

OV2640 capturando frames sob demanda, assincronamente, no Core 0. Detecção básica de presença/ausência. CuriosityService stub.

## ⚠️ Regra crítica

Câmera **obrigatoriamente no Core 0**. Se no Core 1 saturaria FaceRender. Nunca fazer streaming contínuo — apenas janelas sob demanda.

## Critérios de pronto

- 100 capturas sob demanda: FPS da face não cai abaixo de 15 durante nenhuma
- EVT_PRESENCE_DETECTED: entrar e sair do campo detectado em ≤ 3s
- CPU Core 0 ≤ 15% extra durante janela de 500ms de captura

## Testes mínimos

- 100 capturas: verificar que FPS não cai abaixo de 15
- Entrar/sair do campo 5×: verificar EVT_PRESENCE_DETECTED correto
- CPU: medir antes e durante janela de captura

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, OV2640 onboard, Core 0. E36 — CameraService.
Tarefa: camera_service.h/.c.
camera_service_request_frame(): enfileira pedido de captura. CameraCaptureTask (Core 0, P12) captura quando requisitado; resultado em EVT_CAMERA_FRAME{frame_buf, width, height, timestamp}.
Detecção de presença: camera_service_get_frame_diff(frame_prev, frame_curr) → diff_score. Se muda de tier (EMPTY↔OCCUPIED): publicar EVT_PRESENCE_DETECTED{present, confidence}.
BehaviorEngine: solicitar frame via camera_service_request_frame() a cada 5s em IDLE; a cada 10s em ENGAGED.
Restrição: camera_service_request_frame() retorna imediatamente — não bloqueia.
Saída: camera_service.h/.c.
```

```jsx
Contexto: ESP32-S3, OV2640 onboard, esp32-camera / esp_camera, hal_init.h com pinagem fixa da câmera e SCCB compartilhado em GPIO4/5.
Tarefa: camera_service.h + camera_service.c.
Funções: camera_service_init(), camera_service_request_frame(), camera_service_is_ready().
Config: OV2640, grayscale, FRAMESIZE_QQVGA, fb_count=1, CAMERA_GRAB_WHEN_EMPTY, xclk_freq_hz=10000000, buffers próprios em PSRAM para frame atual.
Comportamento: camera_service_request_frame() apenas agenda captura e retorna imediatamente. CameraCaptureTask (Core 0, prioridade 12) captura quando requisitado; resultado em EVT_CAMERA_FRAME{frame_buf, width, height, timestamp_ms}.
Restrições: chamar i2c_master_init() antes de esp_camera_init(); não acessar I2C durante captura; API não pode bloquear.
Saída: serviço de câmera + bring-up com log explícito de init OK ou falha.

```

## ◎ Prompt de Revisão

```
CameraService da E36.
Verificar: (1) captura no Core 0 (não Core 1)? (2) request_frame() não bloqueante? (3) FPS da face não cai durante captura? (4) BehaviorEngine solicita (não polling contínuo)?
Listar problemas.
```

## ✎ Prompt de Correção

```
CameraService com problema: [sintoma — ex: FPS cai durante captura]
Contexto: E36.
Identificar gargalo. Causa + fix.
```

## → Prompt de Continuidade

```
E36 concluída. Câmera assíncrona sem impacto no FPS.
Próxima: E37 (CloudBridge — STT/TTS cloud com timeouts oficiais).
Mostre como implementar CloudBridge não-bloqueante com T_local=300ms, T_soft=600ms, T_hard=1200ms e fallback gracioso.
```


