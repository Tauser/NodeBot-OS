# NodeBot — Memória Operacional do Projeto

Este arquivo existe para manter contexto estável entre sessões.
Use-o como visão rápida do projeto, do hardware real, das decisões já tomadas e dos problemas que já foram investigados.

## 1. Visão geral

- Projeto: robô desktop companion em `ESP32-S3-WROOM N16R8`
- Hardware principal:
  - display `ST7789`
  - câmera `OV2640`
  - microfone `INMP441`
  - amplificador `MAX98357A`
  - `2x SCS0009` via `FE-TTLinker`
  - `WS2812 x2`
  - touch capacitivo
  - IMU
  - SD card
- Filosofia:
  - `offline-first`
  - sem `malloc` em hot path quando possível
  - módulos desacoplados via `EventBus`
  - face procedural estilo EMO via `LovyanGFX` / `LGFX_Sprite`

## 2. Documentos de referência

Antes de propor arquitetura nova, revisar sempre:

- `D:\Projetos\NodeBot\docs\Behavior_Reaction_Development_Plan.md`
- `D:\Projetos\NodeBot\docs\ReactionArchitecture.md`
- `D:\Projetos\NodeBot\docs\Implementation_Process_Log.md`
- `D:\Projetos\NodeBot\docs\Reactions_Behaviors_Gap_Analysis.md`
- `D:\Projetos\NodeBot\docs\Tracker_Alignment.md`
- `D:\Projetos\NodeBot\docs\NVS_Map.md`

Observação:
- o `Implementation_Process_Log.md` é a memória cronológica do que já foi feito
- este `CLAUDE.MD` é a memória operacional resumida

## 3. Pinagem e restrições importantes

Referência principal:
- `D:\Projetos\NodeBot\src\platform\hal_init.h`

Pontos críticos:

- `GPIO43/44` = console UART0
- `GPIO19` = WS2812 externo
- `GPIO20/46` = servos
- `GPIO41/42` = I2S clock/ws
- `GPIO1` = `MAX98357A DIN`
- `GPIO14` = `INMP441 SD`
- `GPIO4/5` = I2C e também barramento físico compartilhado com `OV2640 SCCB`

Consequências:

- IMU em `I2C` usa `GPIO4/5`
- mesmo sem câmera inicializada em software, existe compartilhamento físico nesses pinos
- o barramento de IMU é sensível a fiação, protoboard, pull-up e alimentação

## 4. Estado real do firmware

Grande parte do plano de comportamento já foi implementada até `G1`, incluindo:

- `BehaviorEngine`
- `ReactionResolver`
- `DialogueStateService`
- `PersonaService`
- `GestureService`
- `StateVector` com memória afetiva/comportamental
- `LED router` baseado em baseline + overlay
- intents offline locais

O plano não deve mais ser lido como “faltando tudo”; hoje o projeto está em fase de:

- consolidação
- tuning
- robustez
- integração de voz

## 5. Voz / wake / comandos

Arquivos principais:

- `D:\Projetos\NodeBot\src\svc_audio\wake_word.c`
- `D:\Projetos\NodeBot\src\svc_audio\audio_capture.c`
- `D:\Projetos\NodeBot\src\svc_audio\dialogue_state_service.c`
- `D:\Projetos\NodeBot\src\svc_audio\offline_command_service.c`

### 5.1 Wake word

Situação atual:

- `WakeNet` está funcionando
- foi melhorado com pré-processamento leve em `wake_word.c`:
  - high-pass leve
  - gate leve
  - auto gain por chunk
- isso ajudou bastante a reduzir a necessidade de falar muito alto e muito perto

Importante:

- `DET_MODE_95` foi mantido por enquanto
- se for continuar tuning do wake, o próximo passo natural é reduzir a rigidez do `wakenet_mode`
- mudar uma variável por vez

### 5.2 Comandos offline atuais

As intents offline já implementadas são:

- `para` / `pare`
- `cancela` / `cancelar`
- `silencio`
- `dorme`
- `acorda`
- `volume alto`
- `volume baixo`
- `volume medio`
- `me olha`
- `boa noite`
- `modo privado`
- `sim`
- `nao`
- `ok`

Hoje:

- o `wake word` já funciona por voz
- a interpretação confiável de comandos ainda está mais fechada pelo caminho de teste serial (`cmd ...`)
- o próximo passo desejável é ligar `wake word + frase curta` às intents sem depender do serial

### 5.3 MultiNet / português

Conclusão já levantada:

- `MultiNet` oficial da Espressif é para comandos curtos em vocabulário fechado
- a documentação oficial indica suporte principalmente a `inglês` e `chinês`
- não assumir `pt-BR` nativo como disponível sem validação
- no projeto atual, a API já prevê troca de idioma por usuário via `speech_lang` em NVS
- hoje:
  - `en-US` = caminho de teste viável
  - `pt-BR` = previsto na API, mas sem modelo oficial compatível no pacote atual

Aprendizados importantes desta rodada:

- habilitar `mn5q8_en` exige atenção ao tamanho da partição `model`
- a partição antiga de `0x200000` (2 MB) não comporta o conjunto de modelos ativo
- com `WakeNet + NSNet + VAD + mn5q8_en`, foi necessário ampliar `model` para `0x400000` em `partitions.csv`
- se essa frente for mexida de novo:
  - lembrar que mudar a partição exige regravar tabela de partições e imagem da partição `model`
  - upload incremental só do app não basta

Limitação prática do `mn5q8_en`:

- ele não deve ser tratado como ASR livre para frases arbitrárias
- frases em texto puro como `SLEEP`, `WAKE UP`, `LOOK AT ME` e até frases “boas” em inglês podem falhar com `Misspelling`
- o caminho robusto para `MultiNet5` é:
  - usar comandos preparados via `sdkconfig`
  - ou usar fonemas customizados compatíveis com o modelo

Estado atual da integração:

- existe `speech_command_service.c`
- ele arma uma janela curta após `EVT_WAKE_WORD`
- o modelo inglês de teste atual é `mn5q8_en`
- a integração mais estável desta fase usa `esp_mn_commands_update_from_sdkconfig()`
- o reconhecimento pós-wake deve ser tratado como experimento controlado, não como stack fechada

Subset stock atualmente mais útil para teste:

- `TURN ON THE TV` -> `WAKE`
- `TURN OFF THE TV` -> `SLEEP`
- `HIGHEST VOLUME` -> `VOLUME_HIGH`
- `LOWEST VOLUME` -> `VOLUME_LOW`
- `TURN OFF MY SOUNDBOX` -> `PRIVACY_MODE`
- `TURN OFF ALL THE LIGHTS` -> `GOOD_NIGHT`

Se essa frente for retomada:

- tratar como experimento
- validar comando por comando
- se necessário usar estratégia híbrida em vez de contar com `pt-BR` nativo

## 6. Áudio e feedback

Arquivos:

- `D:\Projetos\NodeBot\src\svc_audio\audio_feedback.c`
- `D:\Projetos\NodeBot\src\svc_audio\audio_playback_task.c`
- `D:\Projetos\NodeBot\src\drivers\audio\audio_driver.c`

Observações importantes:

- há sensação real de delay entre LED e áudio
- isso faz sentido pela arquitetura atual:
  - LED muda quase sincronamente no `led_router`
  - áudio passa por fila/task antes de tocar

Decisão já tomada:

- se essa frente voltar, primeiro tentar reduzir latência do áudio
- não atrasar LED artificialmente como primeira opção

Touch:

- `TAP` e `DOUBLE_TAP` já foram ligados ao `SOUND_CLICK_TOUCH`
- se o clique parecer baixo, suspeitar primeiro do arquivo `.pcm`, não só do ganho global

## 7. Face / expressões / runtime visual

Arquivos principais:

- `D:\Projetos\NodeBot\src\models\face_params.h`
- `D:\Projetos\NodeBot\src\behavior\idle_behavior.c`
- `D:\Projetos\NodeBot\src\face\blink_controller.cpp`
- `D:\Projetos\NodeBot\src\face\face_engine.cpp`
- `D:\Projetos\NodeBot\src\face\face_renderer.cpp`
- `D:\Projetos\NodeBot\src\behavior\behavior_engine.c`

### 7.1 O grande problema já investigado

Havia um bug recorrente de:

- olhos quase fechados
- aparência de “blink eterno”
- face ficando presa num estado pequeno demais

Ele não vinha de uma única causa.
Foi um bug emergente de:

- presets estreitos
- blink
- microefeitos do idle
- composição / target facial
- renderer

### 7.2 O que já foi corrigido

#### Biblioteca de faces

`face_params.h` foi amplamente revisado.
Os presets centrais e problemáticos foram retocados para:

- reduzir colapso visual
- normalizar melhor o spacing
- deixar a família de expressões mais coerente

Famílias já retrabalhadas:

- `NEUTRAL`
- `HAPPY`
- `GLEE`
- `FOCUSED`
- `SQUINT`
- `UNIMP`
- `SLEEPY`
- `SKEPTIC`
- `AWE`
- `WORRIED`
- `SAD_UP`
- `SAD_DOWN`
- `ANNOYED`
- `ANGRY`
- `SCARED`
- `SURPRISED`
- `FRUS_BORED`
- `FURIOUS`

#### Idle behavior

Em `idle_behavior.c`:

- `squint_think` foi suavizado e ficou menos frequente
- `slight_smile` e `squint_think` passaram a pular se os olhos já estiverem pequenos
- `double_blink` também ganhou esse filtro
- timers do idle passaram a ser rearmados ao entrar em `IDLE`, evitando backlog de ações vencidas
- `look_side` e `look_up` foram ajustados visualmente

#### Blink

Em `blink_controller.cpp`:

- foi corrigida reentrada concorrente de blink
- isso foi uma das chaves para estabilizar o bug
- depois o blink passou a usar `face_engine_get_current()` como base mais confiável
- também foi adicionada uma última base aberta segura como fallback

#### Engine / diagnóstico

Em `face_engine.cpp`:

- foi adicionada telemetria de `SMALL_EYES`
- isso permitiu ver casos em que `cur`, `src` e `dst` já estavam presos em algo como `0.08`

Em `behavior_engine.c`:

- existe diagnóstico de `SMALL_EYES`
- existe `FACE_RECOVER` para forçar reaplicação do baseline quando a face parece travada fora de sono/overlay

#### Renderer

Em `face_renderer.cpp`:

- o `highlight` do olho foi desativado
- ele estava criando uma divisão visual interna que fazia o problema parecer pior

### 7.3 Estado atual da frente visual

Situação atual:

- a frente visual está muito mais estável
- o bug dos olhos fechados caiu muito
- o sistema ainda possui telemetria de debug temporária

Se a frente visual for retomada:

- preferir mudanças pequenas e observáveis
- evitar reabrir tuning agressivo em muitas camadas de uma vez
- limpar os logs temporários só depois de confirmar estabilidade por uso real

## 8. LED router

Arquivo:
- `D:\Projetos\NodeBot\src\services\led_router\led_router.c`

Estado da arquitetura:

- LEDs seguem baseline + overlay
- deixaram de refletir microfone cru/VAD
- os 2 LEDs trocam juntos
- overlays temporários são usados para alertas/eventos

Importante:

- âmbar persistente significa `DEGRADED`
- hoje isso pode vir de:
  - `safe_mode`
  - `imu_service_is_degraded()`

Melhoria futura já identificada:

- explicitar no serial o motivo do `LED_STATE_DEGRADED`

## 9. IMU

Arquivos principais:

- `D:\Projetos\NodeBot\src\drivers\imu\imu_driver.c`
- `D:\Projetos\NodeBot\src\services\imu_service\imu_service.c`

Conclusões importantes já obtidas:

- o módulo usado é `MPU6050`
- o driver foi simplificado para `MPU6050-only`
- houve muito trabalho de robustez em leitura, health gating e degradado
- boa parte dos falsos movimentos vinha de ruído elétrico / barramento / módulo

Sinais clássicos já observados:

- leituras inválidas tipo `az=312` / `az=2312`
- `SHAKE` falso em protoboard/fonte ruim

Situação prática:

- comportamento hoje está mais contido
- mas IMU ainda deve ser tratada com cautela na bancada
- em caso de retorno da frente IMU, suspeitar cedo de hardware/fiação/alimentação

Módulos sugeridos para futura troca:

- `LSM6DSOX`
- `BMI270`
- `ICM-42688-P`

## 10. Toque / humor / estado interno

Arquivos:

- `D:\Projetos\NodeBot\src\services\touch_service\touch_service.c`
- `D:\Projetos\NodeBot\src\behavior\state_vector.c`

Observação importante já levantada:

- toque gentil pode manter `HAPPY` por bastante tempo
- isso não é necessariamente bug do preset
- hoje os decays de `mood_valence` e `attention` são relativamente longos

Se essa frente voltar:

- tratar como tuning comportamental
- não como problema visual puro

## 11. Regras práticas para próximas sessões

- antes de mexer em arquitetura, ler os docs em `docs/`
- antes de mexer em face, revisar:
  - `face_params.h`
  - `idle_behavior.c`
  - `blink_controller.cpp`
  - `face_engine.cpp`
  - `behavior_engine.c`
- antes de mexer em wake/voz, revisar:
  - `wake_word.c`
  - `audio_capture.c`
  - `dialogue_state_service.c`
  - `offline_command_service.c`
- manter `docs/Implementation_Process_Log.md` atualizado sempre que houver mudança relevante
- após uma alteração relevante ou uma rodada funcional concluída, fazer commit no GitHub em vez de acumular mudanças grandes demais sem checkpoint
- preferir commits pequenos e descritivos, com escopo claro
- quando uma mudança for experimental, deixar isso explícito no log e no commit
- antes de continuar uma frente já iniciada, revisar o que ficou modificado no worktree para não sobrescrever contexto do usuário
- não tratar protoboard/fonte/fiação como detalhe secundário: no NodeBot, vários bugs já tiveram componente físico real

## 12. Próximos passos úteis já conhecidos

Boas próximas frentes possíveis:

- integrar melhor `wake word + intents offline`
- avaliar MultiNet com cautela, sem assumir `pt-BR` nativo
- reduzir latência percebida do feedback de áudio
- limpar gradualmente os logs temporários de debug visual
- seguir com polimento fino das expressões, com mudanças pequenas

Evitar:

- grandes refactors simultâneos em blink + presets + idle + renderer
- assumir que bug visual é sempre preset; já houve causa de runtime
- reabrir a frente IMU como se fosse só software
