# E08A - Bring-up: Câmera OV2640

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P2
- Risco: MÉDIO
- Depende de: E02

## Bring-up: Câmera OV2640

Complexidade: Média
Depende de: E02
Grupo: Bring-up HW
HW Real: SIM
ID: E08A
Prioridade: P2
Risco: MÉDIO
Status: 🔲 Não iniciada

## Objetivo

OV2640 onboard inicializando via `esp_camera_init()`, capturando frame válido sob demanda e confirmando que a SCCB compartilhada foi configurada na ordem correta.

## Por que agora

Antes de construir `CameraService` assíncrono, vale validar o bring-up cru da câmera e os limites físicos da placa. Isso reduz risco em E36 e evita misturar problema de hardware com problema de arquitetura.

## O que implementar

- Criar bring-up mínimo da OV2640 usando `esp32-camera` / `esp_camera`
- Garantir a sequência `i2c_master_init() -> esp_camera_init()`
- Configurar a câmera onboard com pinagem fixa da placa
- Capturar um frame em grayscale ou JPEG leve apenas para validação
- Logar init OK, resolução e tamanho do buffer capturado
- Confirmar que não há acesso ao barramento I2C durante a captura

## O que NÃO entra

- `CameraService` assíncrono
- detecção de presença
- tracking facial
- integração com cloud

## Critérios de pronto

- `esp_camera_init()` conclui sem erro no hardware real
- uma captura retorna `fb != NULL`
- largura, altura e formato do frame são logados corretamente
- 20 capturas sequenciais funcionam sem reset nem travamento

## Testes mínimos

- `i2c_master_init()` seguido de `esp_camera_init()` sem erro
- capturar 20 frames seguidos com pequeno intervalo e verificar `fb != NULL`
- testar com câmera ativa sem tocar no I2C durante captura
- validar que o restante do sistema continua responsivo após as capturas

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3 com OV2640 onboard, ESP-IDF v5.x, esp32-camera / esp_camera.
Tarefa: criar bring-up mínimo da câmera.

Saída esperada:
- camera_bringup.c/.h ou equivalente
- camera_bringup_init()
- camera_bringup_capture_once()

Regras:
- chamar i2c_master_init() antes de esp_camera_init()
- usar pinagem fixa da OV2640 da placa
- não acessar I2C durante a captura
- registrar logs explícitos de sucesso/falha

Validação:
- capturar frame válido
- logar width, height, len e timestamp
```

## ◎ Prompt de Revisão

```
Bring-up da OV2640.
Verificar: (1) i2c_master_init() ocorre antes de esp_camera_init()? (2) a pinagem usada é a fixa da placa? (3) a API de teste captura frame válido? (4) há acesso indevido ao I2C durante captura? (5) falhas de init são logadas claramente?
Listar problemas.
```

## ✎ Prompt de Correção

```
Bring-up da OV2640 com problema: [sintoma — ex: esp_camera_init falha, frame nulo, travamento após captura].
Contexto: E08A.
Diagnosticar a causa provável e corrigir com o menor ajuste possível.
```

## → Prompt de Continuidade

```
E08A concluída. OV2640 onboard inicializa e captura frames válidos.
Próxima: E36 (CameraService — Captura Assíncrona).
Mostre como transformar o bring-up validado em um serviço não-bloqueante no Core 0.
```
