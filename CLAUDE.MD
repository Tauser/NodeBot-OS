Projeto: robô desktop companion, ESP32-S3-WROOM N16R8 (512KB SRAM, 8MB PSRAM, 16MB flash).
Hardware: ST7789 240x320 (LovyanGFX), OV2640, INMP441, MAX98357A, 2x SCS0009 via FE-TTLinker, IMU, touch, WS2812x2, LiPo 3Ah.
Arquitetura: Core 0 = WiFi/câmera/wake word. Core 1 = face/behavior/áudio/sensores/safety.
Regras: offline-first, sem malloc em hot paths, EventBus entre módulos, face EMO-style procedural 20fps via LGFX_Sprite.
Etapa atual: [E23 — MotionSafetyService]. Último concluído: [e22]. Próximo: [e24].

REGRA DE OURO: CONSULTA OBRIGATÓRIA
Antes de sugerir, criar ou modificar qualquer funcionalidade, você DEVE ler os documentos de referência na pasta /docs (ou raiz). Essas especificações são a "Verdade Absoluta" do projeto e superam qualquer conhecimento genérico.

Sempre baseie suas respostas nestes arquivos específicos:
Comportamento e Estados: BehavioralStateMachine.md e IdleBehaviorEngine.md.
Animações e Faces: AnimationLibrary.md (Contém a Tabela Mestre de 21+ expressões).
Interação Física: Touch&PhysicalReactionSystem.md (Regras para sensores de toque).
Reações Sensoriais: Music&AudioReactionSystem.md e Social&AttentionSystem.md.
Gestão de Energia e Rotina: Power&BatteryBehavior.md e Routine&CircadianSystem.md.
