# Implementation Process Log

Objetivo:
- registrar de forma única tudo o que foi implementado e alterado durante este processo
- facilitar revisão
- evitar perda de contexto
- servir como referência antes de continuar novas alterações

Escopo:
- mudanças de código
- documentos criados
- ajustes de log
- decisões arquiteturais tomadas no processo

Observação:
- este documento descreve o que foi feito até agora
- não representa um changelog definitivo de produto
- serve como memória de trabalho da sessão

---

## 1. Ajustes de comportamento e face

## 1.1 BehaviorEngine

Arquivos:
- `src/behavior/behavior_engine.c`
- `src/behavior/behavior_engine.h`

O que foi feito:
- o `BehaviorEngine` deixou de usar apenas `emotion_mapper` simples e passou a aplicar presets de `face_params_t`
- foi introduzido rastreamento por `face tag`, para evitar reaplicação constante da mesma expressão a cada tick
- foram adicionados presets/tag dinâmicos para:
  - `IDLE_*`
  - `ENGAGED_*`
  - `TALK_*`
  - `SAFE_MODE`
  - `SYSTEM_ALERT`
  - `MUSIC`
- foi adicionada a API:
  - `behavior_engine_refresh_face()`

Motivação:
- separar melhor o baseline da face
- reduzir reset de transição facial
- permitir que o baseline reassuma após ações temporárias do idle

Observação importante:
- mais tarde vimos que essa estratégia de `refresh_face()` também gera re-log frequente de `FACE -> ...`
- isso precisa ser refinado depois, mas a motivação inicial foi correta

Refinamento posterior aplicado:
- `behavior_engine_refresh_face()` deixou de invalidar o `tag` atual
- agora ele apenas força uma reaplicação silenciosa do baseline no próximo tick
- com isso:
  - o baseline continua sendo restaurado após ações do idle
  - o monitor serial deixa de imprimir `FACE -> ...` repetidamente para a mesma face

## 1.2 Remoção do bored como face base persistente

Arquivo:
- `src/behavior/behavior_engine.c`

O que foi feito:
- o comportamento equivalente a `idle bored` deixou de ser tratado como uma face base permanente da árvore

Motivação:
- pelos docs, esse tipo de expressão deveria ser reação/overlay temporário
- não estado facial fixo que só sai com toque

Impacto:
- reduziu o caso em que o robô prendia a face espremida indefinidamente

---

## 2. Ajustes no IdleBehavior

Arquivo:
- `src/behavior/idle_behavior.c`

O que foi feito:
- integração com `behavior_engine_refresh_face()`
- no fim de várias ações do idle, o baseline passou a ser forçado de volta no tick seguinte

Ações afetadas:
- `act_squint_think()`
- `act_slow_blink()`
- `act_slight_smile()`
- `idle_behavior_trigger_yawn()`
- `play_sneeze()`
- `play_hiccup()`

Motivação:
- impedir que a face ficasse parada em um estado temporário depois do fim da animação

## 2.1 Ajuste do squint pensativo

Arquivo:
- `src/behavior/idle_behavior.c`

O que foi feito:
- o squint foi suavizado
- a abertura mínima dos olhos subiu
- offsets de cantos passaram a ser aplicados por olho, de forma correta

Motivação:
- antes o squint colapsava demais o olho
- visualmente parecia uma linha exagerada

## 2.2 Ajuste do olhar lateral

Arquivo:
- `src/behavior/idle_behavior.c`

O que foi feito:
- `look_side` foi ajustado para ficar mais próximo da leitura visual do EMO
- amplitude lateral aumentada
- ida mais rápida
- hold visível
- volta mais suave

Motivação:
- dar mais presença ao olhar lateral sem parecer mecânico

---

## 3. Ajustes no renderer da face

Arquivo:
- `src/face/face_renderer.cpp`

O que foi feito:
- para olhos muito estreitos, a geometria foi simplificada
- curvatura superior/inferior é zerada quando a altura do olho fica muito baixa
- arredondamento é removido em casos extremos
- highlight superior deixa de ser desenhado em olhos muito finos

Motivação:
- remover o artefato visual de linha curva preta sobre olhos quase fechados

Impacto:
- melhor render para squint, blink lento e outros estados de abertura pequena

---

## 4. Ajustes de logs

Arquivos:
- `src/behavior/state_vector.c`
- `src/face/face_engine.cpp`
- `src/behavior/behavior_engine.c`
- `src/svc_audio/vad.c`

O que foi feito:

### StateVector
- log periódico de estado foi rebaixado de `INFO` para `DEBUG`

### FaceEngine
- log periódico de `fps` foi rebaixado de `INFO` para `DEBUG`

### BehaviorEngine
- logs de face passaram a mostrar tags como:
  - `FACE -> IDLE_NEUTRAL`
  - `FACE -> ENGAGED_AWE`
  - etc.

### VAD
- logs muito ruidosos de calibração/periódicos/evento foram comentados

Motivação:
- limpar o monitor serial
- destacar transições mais úteis
- reduzir ruído operacional durante debug do comportamento

Observação:
- os logs de `FACE -> ...` hoje ainda estão mais frequentes do que o ideal por causa do `refresh_face()`

---

## 5. Ajustes no IMUService

Arquivo:
- `src/services/imu_service/imu_service.c`

O que foi feito:
- `FALL_TICKS` aumentou de `2` para `4`
- threshold de shake foi endurecido
- detector ganhou função de reset explícita:
  - `reset_motion_detectors()`
- em erro de leitura da IMU:
  - os detectores são resetados
- leituras implausíveis passaram a ser descartadas:
  - magnitude muito baixa
  - magnitude muito alta
- log de init foi atualizado para refletir os thresholds novos

Motivação:
- reduzir falsos `SHAKE`
- reduzir falsos `FREEFALL`
- proteger a lógica contra leituras espúrias, especialmente em protoboard

Conclusão operacional:
- ajudou a separar problema de detector e problema de hardware
- o firmware ficou mais robusto, mas a protoboard ainda limita confiabilidade da IMU

---

## 6. Documentação criada

## 6.1 Reconhecimento e mapas

Arquivos:
- `docs/NVS_Map.md`

Conteúdo:
- namespaces e chaves de NVS já usadas no projeto
- finalidade de cada grupo de dados persistidos

## 6.2 Análise geral de gaps

Arquivos:
- `docs/Reactions_Behaviors_Gap_Analysis.md`

Conteúdo:
- análise do que falta para reações e comportamentos
- baseada em docs do projeto, plano mestre e código atual

## 6.3 Arquitetura de reação

Arquivos:
- `docs/ReactionArchitecture.md`

Conteúdo:
- arquitetura recomendada para baseline + overlays temporários
- conceito de `FaceCommand`
- prioridade entre camadas
- integração futura de toque, IMU, diálogo e LED

## 6.4 Alinhamento com o tracker

Arquivos:
- `docs/Tracker_Alignment.md`

Conteúdo:
- leitura do rastreamento de etapas exportado do Notion
- diferença entre infraestrutura concluída e comportamento premium ainda pendente

## 6.5 Plano de desenvolvimento

Arquivos:
- `docs/Behavior_Reaction_Development_Plan.md`

Conteúdo:
- plano sequenciado de evolução da camada de comportamento
- prompts prontos por tarefa

---

## 7. Leitura de documentação externa

Arquivos auxiliares criados durante a análise:
- `docs/robot_plano_mestre_v1.1.extracted.txt`

Origem:
- extração textual do `.docx`:
  - `docs/robot_plano_mestre_v1.1.docx`

Motivação:
- facilitar leitura do conteúdo do plano mestre
- permitir busca textual rápida por headings e seções

Observação:
- é um artefato auxiliar de leitura
- pode ser removido depois se você preferir manter `docs/` mais limpa

---

## 8. Principais decisões tomadas no processo

### Decisão 1
- bored/annoyed prolongado não deve dominar a face base por muito tempo

### Decisão 2
- idle temporário precisa devolver controle ao baseline

### Decisão 3
- o pipeline final do produto deve ser mais próximo de:
  - `StateVector -> BehaviorEngine -> FaceCommand -> FaceEngine`
- e não de múltiplos módulos aplicando face diretamente sem arbitragem

### Decisão 4
- a leitura do tracker mostrou que `E33` base já existe
- então o foco real daqui para frente não é "criar o BehaviorEngine"
- é evoluí-lo sem quebrar a fundação já pronta

---

## 9. Estado atual após este processo

O projeto ficou com:
- face base mais estável
- renderer mais robusto
- idle menos sujeito a travar a face
- logs mais utilizáveis
- IMU mais defensiva contra lixo de leitura
- documentação de arquitetura e roadmap muito mais clara

Mas ainda restam pontos a refinar:
- excesso de logs de `FACE -> ...` por reaplicação de baseline
- LEDs ainda não refletindo plenamente o estado comportamental
- reações ricas de toque e IMU ainda não concluídas
- diálogo local mínimo ainda não implementado
- persona e engajamento ainda abertos

---

## 10. Diretriz daqui para frente

Para continuar sem bagunçar a estrutura já existente:
- evitar criar muitos arquivos-base antes da hora
- priorizar evolução incremental dos módulos atuais
- só introduzir novos contratos quando houver encaixe claro no código existente
- documentar cada bloco novo antes de expandir para a próxima etapa

---

## 11. Execução do plano A1

Arquivo criado:
- `src/behavior/face_command.h`

O que foi feito:
- criação do contrato inicial `face_command_t`
- enums de:
  - `face_cmd_source_t`
  - `face_cmd_priority_t`
- struct contendo:
  - `face_params_t params`
  - `issued_ms`
  - `hold_ms`
  - `fade_ms`
  - `priority`
  - `source`
  - `interruptible`
  - flags aditivas para gaze, eyelid e brow

Escopo respeitado:
- nenhuma integração no runtime
- nenhuma refatoração adicional
- nenhuma alteração estrutural fora da tarefa `A1`

---

## 12. Execução do plano A2

Arquivos criados:
- `src/behavior/reaction_resolver.h`
- `src/behavior/reaction_resolver.c`

Arquivo alterado:
- `src/behavior/CMakeLists.txt`

O que foi feito:
- criação do resolvedor central de reações temporárias
- definição de pool fixo com `5` slots, um por categoria:
  - `safety`
  - `dialogue`
  - `physical_reaction`
  - `idle_rare`
  - `idle_occasional`
- criação da API mínima:
  - `reaction_resolver_init()`
  - `reaction_resolver_tick()`
  - `reaction_resolver_submit()`
  - `reaction_resolver_clear()`
  - `reaction_resolver_compose()`
  - `reaction_resolver_get_slot()`
- implementação de expiração por tempo usando `hold_ms + fade_ms`
- implementação de regra simples de substituição por prioridade e `interruptible`
- implementação de composição final a partir de baseline + overlays ativos
- suporte inicial às flags:
  - `additive_gaze`
  - `additive_eyelid`
  - `additive_brow`

Motivação:
- introduzir a peça central prevista na arquitetura de reações
- parar de depender apenas de aplicação direta de face para preparar baseline + overlay
- manter tudo determinístico e sem alocação dinâmica em hot path

Escopo respeitado:
- nenhum chamador novo foi integrado nessa etapa
- nenhuma mudança foi feita em touch, IMU, dialogue ou LEDs
- a entrega ficou restrita ao resolvedor base pedido em `A2`

---

## 13. Execução do plano A3

Arquivos alterados:
- `src/behavior/behavior_engine.c`
- `src/behavior/reaction_resolver.h`
- `src/behavior/reaction_resolver.c`

O que foi feito:
- o `BehaviorEngine` deixou de aplicar o baseline diretamente no `FaceEngine`
- a face base passou a ser materializada como `face_command_t`
- foi adicionado estado interno para:
  - baseline atual
  - flag de baseline sujo
  - instância local de `reaction_resolver_t`
- o loop do `BehaviorEngine` passou a:
  - rodar `reaction_resolver_tick()`
  - compor `baseline + overlays` via `reaction_resolver_compose()`
  - aplicar no `FaceEngine` apenas o comando final composto
- `behavior_engine_refresh_face()` foi preservado
- `behavior_engine_refresh_face()` agora força reaplicação do baseline composto no próximo tick
- foi adicionada a API:
  - `reaction_resolver_has_active()`
- essa API permite manter recomposição enquanto existirem overlays ativos, mesmo sem mudança no baseline

Motivação:
- formalizar o baseline descrito em `ReactionArchitecture.md`
- encaixar o `reaction_resolver` no caminho real de renderização sem reescrever todo o runtime
- preservar a intenção do comportamento atual, mas com a nova espinha arquitetural

Escopo respeitado:
- nenhuma nova reação de toque, IMU ou diálogo foi criada
- o `IdleBehavior` não foi migrado nesta etapa
- a integração ficou limitada ao caminho do baseline do `BehaviorEngine`, como previsto em `A3`

---

## 14. Execução do plano B1

Arquivos criados:
- `src/services/touch_service/touch_interpreter.h`
- `src/services/touch_service/touch_interpreter.c`

Arquivo alterado:
- `src/services/touch_service/CMakeLists.txt`

O que foi feito:
- criação da camada `touch_interpreter` para separar toque cru de toque semântico
- definição dos tipos mínimos:
  - `TOUCH_TYPE_TAP`
  - `TOUCH_TYPE_DOUBLE_TAP`
  - `TOUCH_TYPE_HOLD`
  - `TOUCH_TYPE_PET`
  - `TOUCH_TYPE_ROUGH`
- criação de estado fixo por zona, sem `malloc`
- suporte preparado para `4` zonas:
  - `base`
  - `top`
  - `left`
  - `right`
- criação de API simples:
  - `touch_interpreter_init()`
  - `touch_interpreter_update()`
  - `touch_interpreter_reset_zone()`
- classificação implementada com base em:
  - duração do gesto
  - pico de intensidade observado
  - intervalo entre toques sucessivos
  - contagem de taps em rajada

Regras implementadas:
- `tap` para toque curto
- `double tap` para dois toques rápidos na mesma zona
- `hold` para toque mantido acima da faixa de tap
- `pet` para toque prolongado acima da faixa de hold
- `rough` para rajada de toques curtos com espaçamento curto

Motivação:
- preparar a semântica de toque antes de integrar reação rica por zona
- evitar misturar leitura elétrica com lógica de comportamento
- deixar `B2` mais direto e menos acoplado ao `TouchService`

Escopo respeitado:
- nenhuma integração foi feita ainda com `touch_service`
- nenhum patch foi aplicado em `StateVector`, `BehaviorEngine` ou `reaction_resolver`
- a entrega ficou restrita ao classificador pedido em `B1`

---

## 15. Execução do plano B2

Arquivos alterados:
- `src/services/touch_service/touch_service.h`
- `src/services/touch_service/touch_service.c`
- `src/services/touch_service/touch_interpreter.c`
- `src/behavior/behavior_engine.h`
- `src/behavior/behavior_engine.c`

O que foi feito:
- integração do `touch_interpreter` ao `touch_service`
- o `EVT_TOUCH_DETECTED` passou a carregar semântica de toque:
  - `zone_id`
  - `type`
  - `intensity`
  - `duration_ms`
  - `tap_count`
- a publicação do evento deixou de acontecer no debounce inicial
- agora o evento é publicado quando o gesto termina e pode ser classificado corretamente
- a reação genérica antiga com `SURPRISED` direto foi removida
- o `touch_service` passou a:
  - atualizar `StateVector` por tipo de toque
  - usar `state_vector_on_pet()` em carinho prolongado
  - piorar mais `valence/arousal` em rough repetido
  - selecionar reação facial por zona e tipo
  - submeter overlay facial ao `reaction_resolver` por meio do `BehaviorEngine`
- o `BehaviorEngine` ganhou uma API mínima para enfileirar overlays:
  - `behavior_engine_submit_reaction()`
  - `behavior_engine_clear_reaction()`
- a submissão ao `reaction_resolver` passou a ser processada no tick do `BehaviorLoop`
- o callback de toque do `BehaviorEngine` deixou de reaplicar lógica emocional genérica de toque

Mapeamento aplicado:
- `top`:
  - tap -> happy
  - hold/pet -> happy/glee
  - rough -> annoyed/angry
- `left/right`:
  - tap -> curious/awe
  - hold/pet -> curious/happy
  - rough -> annoyed/angry
- `base`:
  - tap -> shy/embarrassed
  - hold -> shy leve
  - pet -> happy suave
  - rough -> annoyed/angry

Motivação:
- sair do toque genérico único
- começar a refletir a semântica do toque na reação do personagem
- conectar toque ao `reaction_resolver` sem quebrar calibração, NVS e fluxo atual

Observação importante:
- o classificador foi ajustado para não zerar a rajada após `double tap`
- isso permite evoluir naturalmente para `rough` quando houver repetição rápida de toques na mesma zona

Refinamento posterior aplicado:
- foram adicionados logs de teste mais explícitos no `touch_service`
- além do log resumido da classificação, o serial agora mostra:
  - `type`
  - `duration_ms`
  - `intensity`
  - `tap_count`
  - limites usados para interpretar `tap`, `hold`, `pet`, `double tap` e `rough`

Motivação do refinamento:
- facilitar validação manual no hardware com apenas uma zona de touch disponível
- reduzir ambiguidade durante ajuste fino de timing e calibração

Correção posterior aplicada:
- `touch_interpreter_update()` passou a aceitar `out_event == NULL`
- antes disso, as atualizações intermediárias com o dedo ainda pressionado eram abortadas cedo
- isso impedia o interpretador de acumular estado interno corretamente e fazia o touch parecer inativo no fluxo novo

Impacto da correção:
- o interpretador volta a acompanhar o gesto durante `DEBOUNCE` e `ACTIVE`
- a classificação volta a ser emitida corretamente no release

Escopo respeitado:
- nenhuma reescrita completa do `TouchService`
- nenhuma mudança em IMU, LED, diálogo ou persona
- a entrega ficou restrita à integração rica de toque prevista em `B2`

---

## 16. Execução do plano C1

Arquivos criados:
- `src/services/motion_reaction_service/motion_reaction_service.h`
- `src/services/motion_reaction_service/motion_reaction_service.c`
- `src/services/motion_reaction_service/CMakeLists.txt`

Arquivo alterado:
- `src/boot/boot_sequence.c`

O que foi feito:
- criação do `motion_reaction_service`
- assinatura de `EVT_MOTION_DETECTED`
- conversão de eventos da IMU em impacto comportamental e reação facial temporária
- integração com `BehaviorEngine` via `behavior_engine_submit_reaction()`
- inicialização do serviço adicionada ao boot após `imu_service`

Mapeamento implementado:
- `MOTION_SHAKE` leve:
  - aumenta `arousal`
  - aumenta `attention`
  - submete reação curta de surpresa/curiosidade
- `MOTION_SHAKE` forte:
  - reduz `valence`
  - reduz `comfort`
  - aumenta mais `arousal`
  - submete reação mais irritada/angry
- `MOTION_TILT`:
  - aumenta `attention`
  - aumenta `arousal`
  - usa reação confused/skeptic para tilt moderado
  - usa categoria `safety` para tilt mais severo
- `MOTION_FALL`:
  - reduz `comfort`
  - reduz `valence`
  - aumenta `arousal`
  - submete reação `safety` de susto/queda

Motivação:
- adicionar camada comportamental à IMU sem transferir a responsabilidade de proteção
- fazer shake, tilt e fall aparecerem no personagem
- usar o `reaction_resolver` como caminho único para reação facial episódica

Escopo respeitado:
- `motion_safety_service` continua dono da proteção crítica
- nenhum modo degradado foi implementado ainda
- a entrega ficou restrita ao serviço pedido em `C1`

---

## 17. Execução do plano C2

Arquivos alterados:
- `src/services/imu_service/imu_service.h`
- `src/services/imu_service/imu_service.c`
- `src/services/motion_reaction_service/motion_reaction_service.c`

O que foi feito:
- adição de flag pública de degradação da IMU:
  - `imu_service_is_degraded()`
- criação de rastreamento de saúde por janela fixa de amostras
- degradação entra quando a taxa de leituras inválidas/erro fica alta
- recuperação usa histerese com limite menor de inválidas
- `SHAKE` e `TILT` passam a ser suprimidos quando a IMU está degradada
- o `motion_reaction_service` ignora eventos de movimento quando a IMU está degradada
- logging de transição enxuto:
  - `IMU degraded ON`
  - `IMU degraded OFF`
  - supressão de tilt/reação quando aplicável

Motivação:
- evitar reações emocionais falsas em protoboard ou sensor instável
- manter o caminho comportamental pausado quando a confiabilidade da IMU cair
- preservar a camada de segurança sem reescrever o serviço de proteção

Decisão importante:
- a degradação foi separada do reset dos detectores de movimento
- isso evita que a flag degradada seja perdida a cada reset interno dos detectores

Escopo respeitado:
- `motion_safety_service` não foi alterado
- o foco ficou em IMU degradada e pausa do reaction path
- nenhuma outra frente fora de `C2` foi aberta

---

## 18. Execução do plano D1

Arquivos criados:
- `src/services/led_router/led_router.h`
- `src/services/led_router/led_router.c`
- `src/services/led_router/CMakeLists.txt`

Arquivos alterados:
- `src/boot/boot_sequence.c`
- `src/boot/CMakeLists.txt`

O que foi feito:
- criação do `led_router`
- integração do serviço ao boot
- refresh leve periódico com `tick` de 200 ms
- assinatura de:
  - `EVT_WAKE_WORD`
  - `EVT_VOICE_ACTIVITY`
- atualização de LED só quando o estado realmente muda

Precedência implementada:
1. `privacy`
2. `camera active`
3. `listening`
4. `safe_mode / degraded`
5. `normal`

Mapeamento aplicado ao driver existente:
- `privacy` -> `LED_STATE_PRIVACY`
- `camera active` -> `LED_STATE_CAMERA`
- `listening` -> `LED_STATE_LISTENING`
- `safe_mode` ou `imu degraded` -> `LED_STATE_DEGRADED`
- fallback -> `LED_STATE_NORMAL`

Decisões de integração:
- `privacy` e `camera active` foram ligados por hooks fracos
- isso evita criar dependência artificial antes das políticas/serviços definitivos existirem
- `listening` foi modelado com hold curto usando wake word + VAD

Motivação:
- parar de deixar o LED sem roteamento contextual explícito
- refletir estado do sistema com precedência estável
- preparar o terreno para integração futura de camera/privacy sem refazer o serviço

Escopo respeitado:
- nenhuma reescrita do driver WS2812
- nenhuma política de privacidade nova foi criada aqui
- a entrega ficou restrita ao `led_router` pedido em `D1`

Refinamento posterior aplicado:
- brilho padrão dos LEDs reduzido para `25%` (`64/255`)
- ajuste no boot para usar `64` como fallback de `led_bright`
- ajuste no default de NVS para `led_bright = 64`
- `ws2812_set_state()` passou a aplicar o estado aos `2` LEDs externos da fita
- o blink de `LED_STATE_CAMERA` também passou a piscar os `2` LEDs externos, não apenas um índice

Motivação do refinamento:
- adequar intensidade luminosa ao hardware real
- alinhar o comportamento do driver com a presença de `2` LEDs externos
- corrigir a divergência entre comentário/API e implementação efetiva do driver

Refinamento posterior adicional:
- brilho padrão reduzido novamente para uso de bancada
- fallback do boot alterado para `24/255`
- default de NVS alterado para `24/255`

Motivação:
- `64/255` ainda ficou forte demais no ambiente de bancada

Correção posterior aplicada:
- o boot passou a limitar `led_bright` a no máximo `24/255`
- isso evita que um valor antigo persistido no NVS mantenha o brilho acima do nível de bancada desejado

Motivação:
- apenas mudar o default não era suficiente quando o valor já existia salvo

---

## 19. Execução do plano E1

Arquivos criados:
- `src/svc_audio/offline_command_service.h`
- `src/svc_audio/offline_command_service.c`

Arquivos alterados:
- `src/core/event_bus/event_bus.h`
- `src/svc_audio/CMakeLists.txt`
- `src/boot/boot_sequence.c`

O que foi feito:
- criação do `offline_command_service`
- definição de intents offline mínimas:
  - `STOP`
  - `CANCEL`
  - `SILENCE`
  - `SLEEP`
  - `WAKE`
  - `VOLUME_HIGH`
  - `VOLUME_LOW`
  - `VOLUME_MEDIUM`
  - `LOOK_AT_ME`
  - `GOOD_NIGHT`
  - `PRIVACY_MODE`
  - `YES`
  - `NO`
  - `OK`
- criação de evento dedicado:
  - `EVT_OFFLINE_INTENT`
- criação de normalização textual leve para matching determinístico
- suporte a variações ASCII para comandos em português simples
- criação de API pública:
  - `offline_command_match_text()`
  - `offline_command_process_text()`
  - `offline_intent_name()`
- integração mínima com serviços existentes:
  - wake word toca som de ativação
  - volume alto/baixo/médio ajustam `audio_vol` e `audio_set_volume()`
  - `me olha` chama `gaze_service_set_target()`
  - `dorme` / `boa noite` baixam energia/atenção para favorecer transição de comportamento
  - `acorda` aumenta atenção e energia mínima
  - `sim` / `não` / `ok` geram confirmação local simples
- init do serviço integrado ao boot

Decisão importante de escopo:
- esta etapa entrega a camada de intents locais e a execução mínima de comandos
- não foi implementado STT completo nem keyword spotting por comando nesta fase
- o caminho público `offline_command_process_text()` ficou pronto para teste/injeção local e para futura ligação com reconhecimento local real

Motivação:
- cumprir a etapa de comandos offline mínimos sem fingir uma stack de ASR que ainda não existe no firmware
- deixar a arquitetura pronta para encaixar o reconhecimento local depois
- já produzir efeitos reais em serviços existentes

Escopo respeitado:
- não foi criada ainda a FSM completa de diálogo
- nenhuma camada cloud foi adicionada
- a entrega ficou restrita aos comandos offline mínimos e intent mapping local de `E1`

---

## 20. Execução do plano E2

Arquivos criados:
- `src/svc_audio/dialogue_state_service.h`
- `src/svc_audio/dialogue_state_service.c`

Arquivos alterados:
- `src/svc_audio/CMakeLists.txt`
- `src/svc_audio/offline_command_service.c`
- `src/boot/boot_sequence.c`

O que foi feito:
- criação do `dialogue_state_service`
- definição dos subestados internos:
  - `IDLE`
  - `LISTENING`
  - `PROCESSING`
  - `SPEAKING`
- assinatura de:
  - `EVT_WAKE_WORD`
  - `EVT_VOICE_ACTIVITY`
  - `EVT_OFFLINE_INTENT`
- criação de task leve própria com tick de `100 ms`
- integração com `reaction_resolver` por meio de `behavior_engine_submit_reaction()` usando a categoria `dialogue`
- integração com playback pré-gravado via `audio_feedback_is_playing()`

Fluxo implementado:
- `wake word` entra em `LISTENING`
- logo após o wake, um overlay curto de `FACE_AWE` é aplicado
- em seguida o serviço aplica `FACE_FOCUSED` como face de escuta
- quando a fala termina após atividade de voz válida, o serviço entra em `PROCESSING`
- `PROCESSING` usa `FACE_SQUINT`
- quando um `EVT_OFFLINE_INTENT` chega, o serviço entra em `SPEAKING`
- `SPEAKING` usa `FACE_HAPPY` e permanece ativo enquanto houver playback curto local ou até timeout máximo
- se o listening expira sem entendimento, o serviço aplica fallback confuso com `FACE_SKEPTIC`
- se o processing expira sem intent, o serviço também aplica fallback confuso e toca `SOUND_ERROR_TONE`

Hook mínimo adicional:
- `offline_command_service.c` passou a informar explicitamente o caso de "não entendi"
- quando `offline_command_process_text()` não encontra intent válida, ele chama `dialogue_state_service_report_no_understanding()`
- isso fecha o requisito de fallback confuso mesmo antes de existir um STT local completo

Compatibilidade com o runtime atual:
- a FSM macro do `BehaviorEngine` não foi explodida nem reescrita
- o macro estado `TALKING` continua existindo
- o `DialogueStateService` apenas detalha os subcontextos por cima dele
- atenção é mantida alta enquanto o diálogo está ativo e reduzida ao sair, para permitir retorno natural a `ENGAGED`

Refinamento posterior aplicado:
- `offline_command_service` ganhou uma task leve de teste via console serial
- o monitor serial agora aceita:
  - `cmd <frase>`
  - `say <frase>`
- essas entradas chamam `offline_command_process_text()` diretamente
- com isso, o caminho `PROCESSING -> SPEAKING` pode ser validado no hardware real mesmo antes de existir um STT local completo ligado ao pipeline

Motivação do refinamento:
- fechar a validação prática do `DialogueStateService`
- evitar depender de injeção manual em código para testar intents offline
- manter o escopo dentro do `E2`, sem adicionar cloud nem uma stack nova de reconhecimento

Escopo respeitado:
- nenhuma camada cloud foi adicionada
- nenhum STT completo foi criado fora do que `E1` já deixou pronto
- a entrega ficou restrita à camada mínima de diálogo local prevista em `E2`
---

## 21. Refinamento da camada de LED para baseline + overlay

Arquivos alterados:
- `src/services/led_router/led_router.h`
- `src/services/led_router/led_router.c`
- `src/drivers/led/ws2812_driver.h`
- `src/drivers/led/ws2812_driver.c`

O que foi feito:
- o `led_router` deixou de usar atividade crua de mic/VAD como dona do estado visual dos LEDs
- a resolução de LED passou a seguir o modelo:
  - baseline por estado macro do robô
  - overlay temporário para alertas/eventos
  - overrides fortes para `privacy`, `camera active` e `degraded`
- o baseline agora segue `BehaviorEngine`:
  - `SLEEP` -> `LED_STATE_SLEEP`
  - `IDLE` -> `LED_STATE_IDLE`
  - `ENGAGED` -> `LED_STATE_ENGAGED`
  - `TALKING` -> `LED_STATE_TALKING`
  - `SAFE_MODE` -> `LED_STATE_DEGRADED`
- overlays temporários foram adicionados para:
  - `EVT_WAKE_WORD`
  - `EVT_OFFLINE_INTENT`
  - `EVT_SYS_LOWBAT`
  - `EVT_SYS_ERROR`
- cada overlay expira sozinho e o sistema volta ao baseline automaticamente

Padrões visuais adicionados ao driver:
- `LED_STATE_SLEEP` -> azul suave pulsando
- `LED_STATE_IDLE` -> azul sólido
- `LED_STATE_ENGAGED` -> ciano sólido
- `LED_STATE_TALKING` -> âmbar pulsando
- `LED_STATE_ALERT` -> vermelho piscante rápido
- `LED_STATE_DEGRADED` -> âmbar sólido
- `LED_STATE_PRIVACY` -> branco sólido
- `LED_STATE_CAMERA` -> vermelho piscante lento

Decisão importante:
- os `2` LEDs externos continuam sincronizados e trocam juntos
- o LED deixa de refletir captura contínua do microfone e passa a refletir estado semântico do robô

Motivação:
- eliminar a oscilação visual causada por falsos positivos de wake word ou por atividade crua de áudio
- alinhar a camada de LED com a mesma filosofia de baseline + overlay usada na face
- preparar terreno para futuros padrões e animações sem recomeçar a arquitetura

---

## 22. Execução do plano F1

Arquivos criados:
- `src/behavior/persona_service.h`
- `src/behavior/persona_service.c`

Arquivos alterados:
- `src/behavior/CMakeLists.txt`
- `src/behavior/behavior_engine.c`

O que foi feito:
- criação do `persona_service`
- definição de uma camada lenta de tendências persistentes derivadas do `StateVector`
- persona atual passa a manter os seguintes eixos:
  - `bond_bias`
  - `social_drive`
  - `expressiveness`
  - `reserve`
- `affinity` persistida no NVS é usada como base do vínculo da persona
- o serviço usa low-pass suave para evitar mudanças bruscas de personalidade

Integração mínima com o runtime:
- `behavior_engine` passou a inicializar o `persona_service`
- o `BehaviorLoop` chama `persona_service_tick()` a cada ciclo
- os nós de baseline do `BehaviorEngine` passaram a consultar a persona para deslocar thresholds de forma suave

Efeitos práticos no baseline:
- persona mais calorosa (`bond_bias`) facilita transições para `HAPPY` e `GLEE`
- persona mais expressiva facilita `AWE` e expressões mais abertas com menos arousal/valence
- persona mais reservada puxa mais facilmente para `FOCUSED` em vez de expressões abertas
- persona com maior `social_drive` procura contato antes, reduzindo o threshold de `AWE` em idle

Decisão importante:
- a persona não substitui o `BehaviorEngine`
- ela só alimenta a escolha do baseline por meio de bias contínuo
- nenhuma nova FSM foi criada
- nenhuma lógica cloud foi adicionada

Escopo respeitado:
- a entrega ficou restrita ao `PersonaService` e à integração mínima com `state_vector / behavior_engine`
- não houve reescrita dos serviços de touch, diálogo, LED ou memória

---

## 23. Execução do plano F2

Arquivos criados:
- `src/behavior/gesture_service.h`
- `src/behavior/gesture_service.c`

Arquivos alterados:
- `src/behavior/CMakeLists.txt`
- `src/behavior/behavior_engine.c`

O que foi feito:
- criação do `gesture_service`
- definição do contrato mínimo `gesture_command_t`
- o contrato inclui:
  - `target`
  - `pan_delta_deg`
  - `tilt_delta_deg`
  - `duration_ms`
  - `hold_ms`
  - `priority`
  - `source`
  - `interruptible`
- definição de enums de source/priority compatíveis com a hierarquia da face
- criação de slot ativo fixo para gesto, sem `malloc`
- implementação de preempção simples por prioridade e interruptibilidade
- implementação de expiração determinística por tempo

Integração com safety:
- `gesture_service_submit()` rejeita comandos quando `motion_safety_is_safe()` for falso
- o serviço assina `EVT_SERVO_BLOCKED`
- quando houver bloqueio de servo, o gesto ativo é limpo imediatamente
- o `BehaviorLoop` passou a chamar `gesture_service_tick()` para manter o ciclo do gesto determinístico

Decisão importante de escopo:
- esta etapa prepara a camada de gesto desacoplada da face
- não foi implementado movimento real de servo ainda
- o objetivo aqui foi entregar o contrato e a arbitragem segura para a integração futura

Escopo respeitado:
- nenhuma camada de servo real foi inventada fora do que o projeto já possui hoje
- a entrega ficou restrita ao `GestureService` e ao contrato mínimo de `GestureCommand`

---

## 24. Execução mínima do plano G1

Arquivos alterados:
- `src/models/state_vector.h`
- `src/behavior/state_vector.c`
- `src/svc_audio/offline_command_service.c`

O que foi feito:
- `G1` foi tratado no escopo mínimo realmente faltante, sem criar nova arquitetura
- adição de `state_vector_on_voice_intent()` para consolidar ganho de vínculo por comando/voz reconhecida
- integração desse ganho no caminho de intent offline em `offline_command_process_text()`
- adição de `rough_streak` no `StateVector`
- rough repetido passou a reduzir `affinity` após recorrência
- adição de erosão lenta de `affinity` por inatividade total prolongada
- presença passou a dar um reforço pequeno e contínuo em `attention`
- `last_affinity_ms` foi adicionado para controlar o decay social de longo prazo sem quebrar o NVS atual

Motivação:
- fechar as lacunas reais apontadas na auditoria de `G1`
- melhorar memória afetiva e regras de engajamento sem inventar um subsistema novo
- manter compatibilidade com a persistência atual de `affinity`

Decisão importante:
- não foi criado serviço novo de memória
- não foi criada memória episódica
- a entrega ficou restrita a consolidar `affinity`, `attention` e regras sociais já existentes

---

## 25. Diagnóstico e mitigação IMU (runtime instável)

Arquivos alterados:
- `src/platform/hal_init.h`
- `src/drivers/imu/imu_driver.c`

Sintoma observado em runtime:
- vários erros `imu_get_accel_mg(...): accel read`
- muitas amostras descartadas por magnitude muito baixa (`|mag|` ~ 60–100 mg)
- valores com degraus típicos de leitura I2C intermitente/instável

Constatações no código:
- IMU usa `HAL_I2C_SDA=4` e `HAL_I2C_SCL=5`
- esses pinos são fisicamente compartilhados com SCCB da OV2640
- o próprio `hal_init.h` já marca esse barramento como compartilhado e sensível

Mitigação aplicada:
- redução do clock I2C de `400000` para `100000` em `HAL_I2C_FREQ_HZ`
- adição de retry curto no driver IMU para leituras I2C (`IMU_READ_RETRIES=3` + backoff de 2 ms)
- uso desse retry nas leituras de `WHO_AM_I`, aceleração e giroscópio

Objetivo da mitigação:
- reduzir falhas transitórias de barramento em bancada
- aumentar robustez sem alterar arquitetura de serviços

Refino adicional aplicado após novo teste:
- `src/drivers/imu/imu_driver.c` recebeu detecção robusta de `WHO_AM_I` por múltiplas amostras
- o tipo de IMU agora é decidido por maioria (`MPU` vs `ICM`) para reduzir risco de mis-detect no boot
- log de boot da IMU passou a mostrar contagem das amostras de identificação

Mitigação extra para falso positivo de movimento:
- `src/services/imu_service/imu_service.c` foi endurecido para reduzir `SHAKE/TILT/FALL` espúrios com IMU instável
- janela de saúde reduzida para resposta mais rápida:
  - `IMU_HEALTH_WIN: 32 -> 16`
  - `IMU_DEGRADED_INVALID_MIN: 8 -> 3`
  - `IMU_DEGRADED_RECOVER_MAX: 2 -> 1`
- adicionada exigência de sequência mínima de amostras válidas antes de liberar eventos de movimento:
  - `IMU_VALID_STREAK_MIN_EVENTS = 12` (~600 ms)
- se houver leitura inválida/erro, streak e detectores são resetados imediatamente

Refino adicional após persistência de `SHAKE` espúrio:
- degradação passou a ter tempo mínimo ativo antes de recuperar:
  - `IMU_DEGRADED_HOLD_MS = 5000`
- faixa de magnitude considerada válida foi apertada para contexto de bancada:
  - `600 mg <= |mag| <= 1600 mg`
- eventos de movimento agora só são avaliados quando:
  - não está degradado
  - há streak mínimo válido
  - a janela de saúde está limpa (`s_health_invalid == 0`)

Refino fail-safe adicional:
- adicionada `IMU_CLEAN_WINDOW_MS = 5000` no `imu_service`
- qualquer leitura inválida/erro atualiza `s_last_invalid_us`
- `SHAKE/TILT/FALL` só são avaliados após 5 segundos contínuos sem inválidas
- objetivo: impedir falso movimento em barramento ainda instável

---

## 26. Refactor do IMU driver (robustez de leitura)

Arquivos alterados:
- `src/drivers/imu/imu_driver.c`

O que foi feito:
- reescrita do driver com foco em robustez no barramento compartilhado
- criação de probe `WHO_AM_I` por maioria (9 amostras) para reduzir mis-detect em boot instável
- init por chip (MPU6050/ICM42688) sem uso de `ESP_ERROR_CHECK` abortivo
- unificação de leitura por mapa de registradores (`accel_reg`, `gyro_reg`) conforme tipo detectado
- retry curto de I2C nas operações de leitura
- validação de bloco bruto de 6 bytes para rejeitar padrões espúrios (ex.: low bytes zerados em todos os eixos)
- manutenção da API pública existente (`imu_init`, `imu_get_type`, `imu_get_accel_mg`, `imu_get_gyro_dps`)

Objetivo:
- impedir que leituras quantizadas/intermitentes virem sinal de movimento válido
- reduzir falso positivo de `SHAKE` sem quebrar integração com `imu_service`

Correção de build pós-refactor:
- `src/drivers/imu/CMakeLists.txt` estava com `SRCS` vazio
- adicionado `imu_driver.c` em `SRCS` para restaurar link de `imu_init` e `imu_get_accel_mg`

Refino de diagnóstico:
- `src/drivers/imu/imu_driver.c` entrou em modo temporário `IMU_FORCE_TYPE_MPU6050`
- objetivo: eliminar hipótese de mis-detect entre MPU/ICM e validar leitura no caminho fixo do módulo confirmado pelo usuário
- o boot agora loga `whoami raw` mesmo em modo forçado

Simplificação permanente em seguida:
- driver reduzido para `MPU6050-only` (sem caminho `ICM42688`)
- `WHO_AM_I` validado apenas para MPU6050
- leituras de accel/gyro migradas de burst `6 bytes` para leitura por eixo (`2 bytes` por transação)
- objetivo: aumentar tolerância do barramento e remover ambiguidade de chip

Mitigação elétrica por software (barramento instável):
- `HAL_I2C_FREQ_HZ` reduzido de `100000` para `50000`
- `imu_driver.c` com `glitch_ignore_cnt` aumentado (`7 -> 15`)
- pull-up interno do ESP para I2C desabilitado (`enable_internal_pullup=false`) para usar somente pull-ups externos
- motivação: reduzir erro de bit intermitente observado no eixo Z (saltos de ~2000 mg)

Refino de detector após estabilização parcial do barramento:
- `src/services/imu_service/imu_service.c`
- `SHAKE_THRESH_VAR` aumentado de `25000` para `60000`
- adicionado segundo critério para `SHAKE`: excursão pico-a-pico mínima por eixo (`SHAKE_THRESH_P2P = 220 mg`)
- log de `SHAKE` agora inclui `p2p` além de `var` e `intensity`
- objetivo: impedir que jitter residual/ruído lento em bancada pareça shake real

Ajuste de telemetria:
- log de init do `imu_service` atualizado para mostrar thresholds reais de `SHAKE`
- agora o boot imprime `shake_var` e `shake_p2p` coerentes com o firmware carregado

Telemetria adicional de face/idle:
- `src/behavior/idle_behavior.c`
- disparos de `tier2` passaram de `ESP_LOGD` para `ESP_LOGI`
- objetivo: permitir identificar no serial qual behavior de idle foi chamado (`look_side`, `look_up`, `squint_think`, `slow_blink`, `double_blink`, `slight_smile`)

Refino de compatibilidade visual no idle:
- `src/behavior/idle_behavior.c`
- adicionada a helper `eyes_already_small()`
- `act_squint_think()` agora é pulada quando a face atual já está com olhos pequenos
- `act_slight_smile()` também é pulada quando a face atual já está com olhos pequenos
- além do check inicial, ambas revalidam a face após o `settle` de `250 ms`
- `act_double_blink()` também passou a ser pulada quando a face atual já chega com olhos pequenos
- `src/face/blink_controller.cpp` passou a pular blink automático e one-shot quando a face base já está com baixa abertura
- `idle_task()` passou a rearmar todos os timers na entrada em `BSTATE_IDLE`
- `act_look_side()` teve amplitude lateral aumentada (`0.50 -> 0.68`) e hold levemente maior
- `act_look_up()` teve amplitude vertical aumentada (`-0.28 -> -0.40`) e desvio horizontal ampliado
- `act_look_side()` passou a aplicar um efeito leve de profundidade:
  - o olho do lado do olhar fecha um pouco
  - o olho oposto abre ligeiramente
  - a assimetria é restaurada ao voltar o gaze para o centro
- `gaze_service` teve o clamp ampliado (`±0.80 -> ±0.92`)
- `face_engine` passou a converter gaze para um deslocamento maior em tela (`x: 25 -> 32 px`, `y: 15 -> 17 px`)
- `act_look_side()` ficou mais suave e mais amplo:
  - alvo lateral maior (`0.68 -> 0.82`)
  - ida mais longa
  - retorno mais suave
  - hold lateral ligeiramente maior
- o efeito de profundidade do `look_side()` foi suavizado:
  - agora só entra quando ambos os olhos estão bem abertos
  - a compressão do olho do lado do olhar foi reduzida (`0.84 -> 0.94`)
  - a abertura do olho oposto também ficou mais sutil (`1.04 -> 1.02`)
- o parallax do `look_side()` deixou de ser aplicado instantaneamente
- agora ele cresce de forma progressiva conforme o `gaze` se desloca lateralmente
- na volta ao centro, o efeito também se desfaz gradualmente antes do restore final
- `act_squint_think()` foi suavizado:
  - fechamento dos olhos reduzido (`0.68 -> 0.80`)
  - deltas de canto reduzidos pela metade
  - transição de ida/volta ficou mais lenta
  - hold no auge ficou menor
- frequência do `squint_think` reduzida:
  - intervalo passou de `12–25 s` para `22–45 s`

Motivação:
- evitar “olhos misteriosos” ou colapso visual quando um behavior ocasional tenta apertar ainda mais uma face que já chegou com abertura pequena
- impedir deformação perceptível em estados como `glee` quando `slight_smile` é aplicado por cima
- fechar o último caminho residual em que `double_blink` ainda podia empurrar uma face já estreita para um visual estranho
- impedir rajadas de `tier2` logo após voltar para `IDLE`, causadas por timers vencidos acumulados enquanto o robô estava em outro estado
- dar mais presença visual aos movimentos de olhar em volta, aproximando o idle do efeito esperado no robô
- sugerir profundidade no olhar lateral sem mexer no renderer base
- permitir que os olhos cubram uma área maior da tela no movimento lateral sem perder coesão visual
- evitar o reaparecimento de “olhos fechados” causado por um parallax forte demais sobre certas expressões base
- deixar o movimento mais orgânico, fazendo a deformação lateral acompanhar a trajetória do olhar
- deixar o `squint_think` menos artificial e menos repetitivo no idle

Integração de áudio no toque:
- `src/services/touch_service/touch_service.c`
- `audio_feedback_play(SOUND_CLICK_TOUCH)` ligado no caminho de `TOUCH_TYPE_TAP` e `TOUCH_TYPE_DOUBLE_TAP`
- objetivo: reproduzir `click_touch.pcm` ao toque curto, alinhando a UX com o SD já carregado

Correção de build da integração:
- `src/services/touch_service/CMakeLists.txt`
- adicionado `svc_audio` em `REQUIRES` para expor `audio_feedback.h` ao componente `touch_service`

Refino de volume do toque:
- `src/svc_audio/audio_playback_task.c`
- `SOUND_CLICK_TOUCH` passou a receber ganho específico de `200%` antes do playback
- o ganho é aplicado com saturação (`clamp`) para evitar overflow/estouro de sample
- os demais sons continuam usando o volume global normal

Motivação:
- o `click_touch.pcm` do SD estava audível, mas baixo demais em comparação com o restante do sistema
- o ajuste foi isolado no toque para não deixar `wake`, `error` e `ack` exagerados

Correção de crash no diálogo:
- `src/svc_audio/dialogue_state_service.c`
- `set_state()` deixou de fazer `ESP_LOGI()` dentro de região crítica
- as transições de estado agora são aplicadas sob `taskENTER_CRITICAL`, mas o log é emitido só depois do `taskEXIT_CRITICAL`

Motivação:
- evitar abort em `lock_acquire_generic` do `newlib`
- o crash ocorria quando `WAKE_WORD` entrava em `LISTENING` e o callback fazia log enquanto ainda segurava o lock do serviço

Correção de timeout no RMT dos LEDs:
- `src/drivers/led/ws2812_driver.c`
- acesso ao `rmt_transmit/rmt_tx_wait_all_done` foi serializado via mutex do driver
- criada a helper interna `ws2812_show_locked()`
- o timer de animação agora só atualiza os LEDs se conseguir pegar o mutex sem bloquear
- `ws2812_set_state()` e callbacks periódicos deixaram de competir entre si pelo mesmo canal RMT

Motivação:
- evitar `rmt_tx_wait_all_done(...): flush timeout` durante wake/dialogue e outras trocas rápidas de estado visual
- o problema vinha de transmissões concorrentes do timer de animação e da troca imediata de estado pelo `led_router`

Revisão inicial de presets centrais/problemáticos:
- `src/models/face_params.h`
- `FACE_NEUTRAL`
  - espaçamento inter-ocular suavizado (`x_off: 93 -> 108`)
- `FACE_GLEE`
  - abertura aumentada (`0.28 -> 0.34`)
  - offset vertical reduzido (`-18 -> -14`)
  - corners suavizados
  - espaçamento trazido para mais perto do neutro (`128 -> 118`)
  - transição levemente maior (`400 -> 420 ms`)
- `FACE_SQUINT`
  - abertura aumentada (`0.40 -> 0.48`)
  - corners reduzidos
  - espaçamento trazido para mais perto do neutro (`92 -> 108`)
  - fundo menos duro (`rb: 4 -> 6`)
  - transição levemente maior (`400 -> 420 ms`)

Motivação:
- iniciar a limpeza dos presets mais influentes no runtime
- reduzir saltos de spacing entre estados próximos
- evitar que `GLEE` e `SQUINT` já nasçam “apertados demais” antes mesmo de blink/idle/gaze entrarem em cena

Refino da segunda camada de presets base:
- `src/models/face_params.h`
- `FACE_FOCUSED`
  - abertura aumentada (`0.44 -> 0.50`)
  - cantos suavizados
  - spacing aproximado do neutro (`128 -> 112`)
  - transição levemente maior (`350 -> 380 ms`)
- `FACE_UNIMP`
  - abertura aumentada (`0.34/0.43 -> 0.42/0.48`)
  - cantos reduzidos
  - spacing trazido para mais perto do neutro (`128 -> 112`)
  - offset vertical suavizado (`-6 -> -4`)
  - transição levemente mais lenta (`500 -> 540 ms`)
- `FACE_SLEEPY`
  - abertura do olho menor aumentada (`0.30 -> 0.40`)
  - assimetria preservada, mas menos extrema
  - corners suavizados
  - spacing aproximado do neutro (`93 -> 106`)
  - offset vertical menos agressivo (`-13 -> -10`)
  - transição levemente maior (`800 -> 860 ms`)

Motivação:
- reduzir a sensação de “olhos fechando” em estados de baixa energia/foco
- aproximar esses presets do mesmo idioma visual do `NEUTRAL`
- manter a personalidade de cada estado sem deixar o runtime colapsar para uma leitura de semi-blink

Refino da camada de carisma/expressividade:
- `src/models/face_params.h`
- `FACE_HAPPY`
  - abertura aumentada (`0.32 -> 0.38`)
  - spacing reduzido do extremo (`128 -> 116`)
  - `y` menos agressivo (`-10 -> -8`)
  - curvatura suavizada
- `FACE_SKEPTIC`
  - assimetria preservada, mas menos extrema
  - olho mais fechado abriu um pouco (`0.69 -> 0.76`)
  - spacing trazido para mais perto do neutro (`128 -> 116`)
  - `y` reduzido (`8 -> 6`)
  - corners suavizados
- `FACE_AWE`
  - spacing menos extremo (`89 -> 102`)
  - cantos inferiores menos abertos
  - raio superior reduzido (`36 -> 30`)
  - transição levemente maior (`500 -> 520 ms`)

Motivação:
- preservar carisma e leitura emocional forte sem criar saltos grandes demais em relação ao `NEUTRAL`
- tornar `HAPPY`, `SKEPTIC` e `AWE` mais coesos no runtime, especialmente quando combinados com gaze e idle behaviors

Guardrail de baseline contra “blink eterno”:
- `src/behavior/behavior_engine.c`
- adicionada `normalize_baseline_face()`
- para baselines normais (fora de `SLEEP`, `IDLE_SLEEPY` e `IDLE_UNIMP`):
  - `open_l` e `open_r` não descem abaixo de `0.52`
  - `x_off` não desce abaixo de `104`
- a normalização é aplicada dentro de `set_face()` antes do baseline entrar no pipeline final

Motivação:
- havia reaparição de olhos parecendo “blink eterno” mesmo após suavizar idle e blink
- isso indica que parte do problema ainda vinha de presets base estreitos demais no runtime
- o guardrail preserva estados de sono, mas impede colapso visual em estados normais

Refino da família emocional negativa:
- `src/models/face_params.h`
- `FACE_WORRIED`
  - abertura levemente aumentada (`0.84 -> 0.88`)
  - spacing trazido para mais perto do neutro (`94 -> 108`)
  - inclinação superior suavizada
  - transição levemente maior (`500 -> 520 ms`)
- `FACE_SAD_UP`
  - abertura aumentada (`0.72 -> 0.76`)
  - spacing aproximado do novo centro visual (`128 -> 112`)
  - cantos reduzidos
  - `y` menos agressivo (`-8 -> -6`)
  - transição levemente maior (`600 -> 620 ms`)
- `FACE_SAD_DOWN`
  - abertura aumentada (`0.62 -> 0.68`)
  - spacing aproximado do novo centro visual (`128 -> 112`)
  - cantos suavizados
  - `y` menos pesado (`40 -> 34`)
  - transição levemente maior (`600 -> 620 ms`)

Motivação:
- fechar a revisão dos estados negativos mais usados no runtime
- preservar tristeza e preocupação sem deixar a família nascer “semi-fechada”
- alinhar `WORRIED`, `SAD_UP` e `SAD_DOWN` ao mesmo idioma visual construído nas rodadas anteriores

Refino da família de alta intensidade:
- `src/models/face_params.h`
- `FACE_ANNOYED`
  - abertura aumentada (`0.50/0.60 -> 0.56/0.64`)
  - spacing reduzido do extremo (`128 -> 116`)
  - inclinação e curvatura suavizadas
  - transição levemente maior (`400 -> 430 ms`)
- `FACE_ANGRY`
  - spacing trazido para fora da zona mais extrema (`92 -> 106`)
  - inclinação superior suavizada (`30 -> 24`)
  - abertura levemente aumentada (`0.86 -> 0.90`)
  - transição levemente maior (`400 -> 430 ms`)
- `FACE_SCARED`
  - spacing menos comprimido (`91 -> 104`)
  - abertura aumentada (`0.94 -> 0.98`)
  - corners inferiores suavizados
  - `cv_bot` menos agressivo (`-12 -> -10`)
  - transição levemente maior (`200 -> 230 ms`)

Motivação:
- manter intensidade emocional sem “quebrar” o rosto no runtime
- reduzir saltos visuais muito bruscos em relação ao novo centro do sistema
- preservar leitura forte de `ANNOYED`, `ANGRY` e `SCARED` com geometria mais coesa

Refino dos presets secundários sensíveis:
- `src/models/face_params.h`
- `FACE_SURPRISED`
  - spacing reduzido do extremo (`128 -> 112`)
  - corners levemente suavizados
  - transição um pouco menos brusca (`250 -> 280 ms`)
- `FACE_FRUS_BORED`
  - abertura aumentada (`0.46 -> 0.54`)
  - `y` menos pesado (`40 -> 32`)
  - spacing aproximado do centro visual (`128 -> 112`)
  - cantos suavizados
  - transição levemente maior (`600 -> 620 ms`)
- `FACE_FURIOUS`
  - spacing saiu da zona mais extrema (`92 -> 104`)
  - inclinação e corners inferiores suavizados
  - abertura levemente aumentada (`0.92 -> 0.94`)
  - transição levemente maior (`300 -> 320 ms`)

Motivação:
- fechar a família secundária que ainda podia destoar do novo miolo visual
- manter impacto dramático sem saltos geométricos muito violentos no runtime

Refino de ritmo das transições entre macroestados:
- `src/behavior/behavior_engine.c`
- tempos ajustados nos estados mais frequentes de:
  - `IDLE`
  - `ENGAGED`
  - `TALKING`
  - `SYSTEM_ALERT`
  - `SLEEP`
  - `MUSIC`
- direção geral do tuning:
  - `TALK_LISTEN` e `TALK_THINK` ficaram menos secos
  - `ENGAGED_AWE/GLEE/HAPPY/FOCUSED` ficaram mais suaves
  - `IDLE_*` ficou com retornos e derivações mais orgânicos
  - estados de baixa energia e tristeza ganharam um pouco mais de inércia

Motivação:
- com os presets já estabilizados, o maior ganho seguinte passou a ser a sensação da passagem entre estados
- a intenção foi reduzir “snaps” visuais e dar mais continuidade ao rosto em uso real

Recuo temporário do tuning de transições para isolamento:
- `src/behavior/behavior_engine.c`
  - tempos de transição dos macroestados retornados aos valores anteriores

Motivação:
- após a rodada de timing, houve nova incidência rápida dos “olhos fechados”
- o recuo foi feito para validar se a permanência maior em estados interpolados estava contribuindo para o artefato

Telemetria no pipeline de render da face:
- `src/face/face_engine.cpp`
  - adicionada detecção `params_small_eyes()`
  - adicionada `log_small_eyes_diag()` com throttle de `1500 ms`
  - quando o frame interpolado entra na zona suspeita, o serial agora mostra:
    - `cur` (`open_l`, `open_r`, `x_off`)
    - `src` (`open_l`, `open_r`, `x_off`)
    - `dst` (`open_l`, `open_r`, `x_off`)
    - progresso temporal `elapsed / transition_ms`

Motivação:
- a incidência voltou mesmo após recuar o tuning de transições
- a próxima confirmação necessária é saber se o artefato já nasce em `src/dst` ou se aparece só no estado interpolado `cur`

Recovery defensivo contra face travada em blink:
- `src/behavior/behavior_engine.c`
  - adicionada `face_params_are_stuck_blink()`
  - a cada compose, o engine consulta `face_engine_get_target()`
  - se o target estiver travado com olhos quase fechados (`<= 0.12`) fora de estados de sono e sem overlays ativos:
    - loga `FACE_RECOVER`
    - força reaplicação do baseline

Motivação:
- o log mostrou `src`, `dst` e `cur` todos presos em `0.08`, o que indica travamento no `FaceEngine` e não só no baseline do comportamento
- essa proteção evita que o rosto fique preso nesse estado enquanto continuamos refinando a causa raiz

Correção do blink para usar base renderizada estável:
- `src/face/face_engine.h/.hpp/.cpp`
  - adicionada API `face_engine_get_current()` / `FaceEngine::getCurrent()`
  - expõe `_params` (estado renderizado atual), não só `_dst`
- `src/face/blink_controller.cpp`
  - blink passa a usar `face_engine_get_current()` como base principal
  - adicionada memória `s_last_open_base`
  - se o estado atual estiver pequeno demais, o blink tenta fallback para a última base aberta conhecida
  - `blink_controller_trigger()` também passa a memorizar base aberta a partir do frame atual

Motivação:
- o log indicou travamento do target inteiro em estado de quase-blink
- a hipótese mais forte passou a ser o blink salvando/restaurando uma base volátil ou já contaminada
- esta mudança faz o blink partir de um estado visual real e usar restauração mais segura

Refino do raster dos olhos para reduzir serrilhado:
- `src/face/face_renderer.cpp`
  - `fillEyeColumns()` passou a operar com `yt_f/yb_f` em float até a etapa final
  - testada borda parcial por coluna para antialias simples
  - recuo aplicado após artefato de linha no topo do olho
  - mantida só a base mais segura do cálculo em float
  - highlight superior temporariamente desativado

Motivação:
- suavizar serrilhado nas bordas superior e inferior dos olhos sem mudar a linguagem visual
- melhorar a leitura em diagonais e curvas leves sem custo alto de arquitetura
- o highlight estava criando uma divisao interna entre a parte de cima e o corpo do olho

Isolamento temporário dos microefeitos para depuração dos “olhos fechados”:
- `src/behavior/idle_behavior.c`
  - adicionado `s_disable_idle_micro_effects = true`
  - `squint_think`, `slow_blink`, `double_blink` e `slight_smile` agora apenas logam skip em modo de teste
- `src/face/blink_controller.cpp`
  - adicionado `s_disable_auto_blink_test = true`
  - o blink automático/manual fica temporariamente suprimido para teste

Motivação:
- os “olhos fechados” continuavam reaparecendo mesmo após várias suavizações
- antes de continuar calibrando presets no escuro, ficou mais seguro isolar blink e microtweaks do idle
- isso permite verificar se a causa restante está no baseline/pipeline principal ou nesses microefeitos

Telemetria de diagnóstico para “olhos pequenos” no pipeline final:
- `src/behavior/behavior_engine.c`
  - adicionada detecção `face_params_are_small_eyes()`
  - adicionada `log_small_eyes_diagnostic()` com throttle de `1500 ms`
  - quando a face final composta entra na zona suspeita, o serial agora mostra:
    - tag baseline atual
    - source do comando final
    - `open_l`, `open_r` e `x_off`
    - overlays ativos por categoria, com prioridade e flags aditivas

Motivação:
- com blink e microefeitos temporariamente suprimidos, o próximo passo seguro era enxergar o pipeline final sem adivinhação
- isso deve mostrar se os “olhos fechados” restantes vêm do baseline puro ou de algum overlay ainda ativo

Reativação gradual dos microefeitos:
- `src/behavior/idle_behavior.c`
  - `squint_think` e `slight_smile` reativados
  - `slow_blink` e `double_blink` continuam bloqueados por `s_disable_idle_blinks_test = true`

Motivação:
- com o problema ausente durante o isolamento total, a etapa seguinte mais segura é religar primeiro os microefeitos não-blink
- isso ajuda a separar “micro expressão” de “piscada/fechamento” como causa do bug

Reativação progressiva do blink ocasional:
- `src/behavior/idle_behavior.c`
  - `double_blink` reativado
  - `slow_blink` continua bloqueado
- `src/face/blink_controller.cpp`
  - blink automático/manual global continua em modo de teste

Motivação:
- subir a investigação em degraus pequenos
- validar se o problema aparece com um gatilho de blink ocasional controlado antes de religar o blink automático do sistema inteiro

Reativação do motor de blink:
- `src/face/blink_controller.cpp`
  - `s_disable_auto_blink_test = false`
  - blink automático/manual global reativado
- `src/behavior/idle_behavior.c`
  - `slow_blink` continua desligado

Motivação:
- validar o caminho principal de blink sem ainda reintroduzir o gesto mais pesado de sonolência
- isolar se o problema volta com o motor de blink em si ou apenas com o `slow_blink`

Correção do motor de blink por reentrada concorrente:
- `src/face/blink_controller.cpp`
  - adicionado lock `s_blink_lock`
  - adicionado flag `s_blink_in_progress`
  - `do_blink()` agora faz `blink_try_begin()` e aborta se já houver blink em andamento
  - `blink_end()` garante liberação ao terminar, ao abortar por suppressão e ao pular por olhos já pequenos

Motivação:
- ao religar o motor de blink, os “olhos fechados” voltaram imediatamente
- a hipótese mais forte passou a ser sobreposição de blinks (automático + trigger), capturando uma base já semi-fechada
- essa correção isola o blink para um único fluxo por vez

Tuning leve do caminho do wake word para melhorar disparo com fala menos forte:
- `src/svc_audio/wake_word.c`
  - adicionado pré-processamento barato só no feed do WakeNet:
    - remoção leve de DC / high-pass de baixa frequência
    - noise gate baixo para limpar hiss/piso muito pequeno
    - ganho adaptativo por chunk para fala mais fraca
  - log de init do wake agora informa `preproc=hp+gate+auto_gain`
  - mantido `DET_MODE_95` por enquanto para não misturar duas variáveis antes de validar o efeito do pré-processamento

Motivação:
- o wake estava funcional, mas exigindo fala muito alta e muito perto
- a meta desta rodada foi ajudar a fala útil a chegar mais limpa/forte no WakeNet sem alterar o VAD nem o áudio cru consumido por outros subsistemas
- o ajuste foi mantido leve e reversível para facilitar comparação no hardware

Skills locais do projeto criadas em `skills/`:
- `skills/nodebot-debug`
- `skills/nodebot-face`
- `skills/nodebot-voice`

Conteúdo:
- cada skill recebeu `SKILL.md` enxuto e referências curtas
- foco em continuidade operacional do projeto, não em instruções genéricas
- as skills consolidam:
  - contexto de hardware e pinagem
  - frente visual / face
  - frente de wake / voz / intents offline

Motivação:
- evitar reexplicar o histórico do projeto a cada nova sessão
- reduzir risco de reabrir bugs já investigados
- tornar o conhecimento do NodeBot versionado e local ao repositório

Melhorias futuras anotadas a partir da validação em hardware:
- Wake / comandos por voz
  - manter anotado que o wake melhorou bastante com `preproc=hp+gate+auto_gain`, mas ainda pode valer uma segunda rodada reduzindo a rigidez do `wakenet_mode`
  - investigar enum/modo menos conservador que o `DET_MODE_95`, trocando uma variável por vez
  - ligar um caminho simples de `wake word + frase curta` para alimentar as intents offline sem depender do serial
  - manter como referência a tabela atual de intents offline já implementadas:
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

- Feedback audiovisual
  - investigar a sensação de atraso entre troca de LED e saída de áudio
  - hipótese atual: LED responde no caminho síncrono do `led_router`, enquanto áudio passa por fila/task em `audio_feedback` + `audio_playback_task`
  - próxima melhoria sugerida: reduzir latência do feedback de áudio antes de atrasar LED artificialmente

- Logs / diagnóstico
  - quando estabilizar completamente a frente visual, revisar e limpar telemetrias provisórias:
    - `FACE: SMALL_EYES ...`
    - `BENG: SMALL_EYES ...`
    - `FACE_RECOVER ...`
  - considerar reduzir spam de logs de IMU inválida quando o sensor entrar em degradado prolongado

- Energia / comportamento
  - toque gentil hoje pode manter `HAPPY` por bastante tempo por causa do decay longo de `mood_valence` e `attention`
  - anotar como possível tuning futuro: manter resposta positiva do toque, mas fazer o retorno a `NEUTRAL` acontecer mais cedo

- Alimentação / saúde do sistema
  - anotar observação de bancada: troca de cabo/fonte alterou comportamento de IMU/degraded e também coincidiu com reaparições do bug visual
  - próxima melhoria sugerida: explicitar melhor no serial o motivo do LED âmbar/degraded (`imu degraded` vs `safe mode`)

Refino incremental em wake/comandos offline:
- `src/svc_audio/offline_command_service.c`
  - o matcher de intents offline deixou de depender só de igualdade exata e passou a aceitar a frase alvo dentro de um comando curto maior, respeitando fronteiras de tokens
  - isso permite casos como prefixo de wake ou fala curta mais natural, por exemplo:
    - `... para`
    - `... cancela`
    - `... volume alto`
    - `... olha para mim`
  - foram adicionadas algumas variantes naturais de baixa ambiguidade:
    - `fica em silencio`
    - `vai dormir`
    - `pode dormir`
    - `pode acordar`
    - `olha pra mim`
    - `olha para mim`

- `src/svc_audio/audio_feedback.c`
  - `audio_feedback_is_playing()` agora também considera itens já enfileirados para tocar, não apenas o trecho em reprodução efetiva
  - isso reduz a defasagem percebida entre estado visual e estado de diálogo quando o áudio ainda está aguardando a `AudioPlaybackTask`

Motivação:
- preparar melhor a camada offline para a futura ponte `wake word + frase curta` sem prometer uma stack de ASR ainda inexistente no firmware
- reduzir um pouco a sensação de atraso entre LED/diálogo e feedback de áudio sem introduzir atraso artificial no visual

Refino no wake word para evitar retrigger logo após uma detecção válida:
- `src/svc_audio/wake_word.c`
  - após publicar `EVT_WAKE_WORD`, o módulo agora entra automaticamente em período refratário de `1800 ms`
  - a intenção é bloquear retrigger por:
    - cauda da própria fala do usuário
    - eco ambiente imediato
    - som de ativação disparado logo após o wake
  - os logs de init e de detecção passaram a informar explicitamente o `refractory`

Motivação:
- o detector estava funcional, mas ainda havia espaço para rearmar cedo demais depois de uma detecção válida
- esse ajuste é deliberadamente conservador: não mexe no `DET_MODE_95`, não muda o pré-processamento e não depende de ASR

Infraestrutura da ponte `wake word + frase curta`:
- `src/svc_audio/speech_command_service.c`
- `src/svc_audio/speech_command_service.h`
- `src/svc_audio/audio_capture.c`
- `src/boot/boot_sequence.c`
- `src/svc_audio/offline_command_service.c`

O que foi feito:
- criada uma camada dedicada para reconhecimento local de comandos curtos após o wake
- essa camada:
  - arma uma janela curta ao receber `EVT_WAKE_WORD`
  - consome os mesmos blocos de áudio capturados em `AudioCaptureTask`
  - tenta reconhecer uma intent curta e, se reconhecer, chama `offline_command_process_intent()`
- o `offline_command_service` ganhou o caminho público `offline_command_process_intent()` para permitir integração direta por ID de intent, sem depender de texto intermediário

Estado atual importante:
- o código da ponte ficou pronto, mas o `sdkconfig` atual ainda está com `CONFIG_SR_MN_* = NONE`
- nesse estado, o boot passa a logar explicitamente que `speech_cmd` está desabilitado/indisponível
- isso evita a falsa impressão de que a frase pós-wake já está ativa quando, na prática, ainda falta habilitar um modelo `multinet`

Motivação:
- preparar a arquitetura correta para `wake + frase curta` sem misturar isso dentro do módulo de wake
- deixar a bancada mais objetiva: distinguir claramente entre "wake detectou" e "não existe modelo de command recognition carregado"

Ativação de baseline em inglês e preparação para troca de idioma:
- `sdkconfig`
- `sdkconfig.defaults`
- `src/core/config_manager/nvs_defaults.h`
- `src/svc_audio/speech_command_service.c`
- `src/svc_audio/speech_command_service.h`
- `src/svc_audio/offline_command_service.c`
- `src/svc_audio/offline_command_service.h`

O que foi feito:
- o projeto passou a apontar para `mn5q8_en` como baseline de teste para command recognition offline
- a preferência de idioma de speech commands agora é persistida em NVS por `speech_lang`
  - `0` -> off
  - `1` -> en-US
  - `2` -> pt-BR
- o `speech_command_service` deixou de depender de macro única fixa e passou a selecionar o modelo em função:
  - do idioma configurado pelo usuário
  - dos modelos efetivamente habilitados no `sdkconfig`
- `pt-BR` já ficou previsto na API e no storage, mas segue com fallback explícito por ainda não haver `multinet` oficial compatível no pacote atual
- o console serial de teste ganhou:
  - `lang`
  - `lang off`
  - `lang en`
  - `lang ptbr`
- o matcher textual local também passou a aceitar equivalentes em inglês para facilitar testes alinhados ao `mn5q8_en`

Motivação:
- destravar testes reais de `wake + phrase` agora, sem bloquear a arquitetura futura de internacionalização
- deixar a mudança de idioma ser uma decisão do usuário, não uma constante enterrada no firmware
- 2026-03-28
- diagnóstico do crash de boot após habilitar `mn5q8_en`
- causa provável isolada: a partição `model` estava em `0x200000` (2 MB), mas o conjunto ativo de modelos ultrapassa isso com folga
- ordem de grandeza dos blobs principais:
  - `mn5q8_en` ~2.18 MB
  - `nsnet1` ~0.82 MB
  - `vadnet1_medium` ~0.29 MB
  - `wn9_hiesp` ~0.29 MB
  - `wn9_jarvis_tts` ~0.29 MB
- efeito observado em bancada:
  - boot panic em `get_model_info()` / `srmodel_load()` dentro de `esp_srmodel_init("model")`
  - assinatura compatível com partição de modelos inválida/truncada
- correção aplicada:
  - `partitions.csv`: `model` aumentado de `0x200000` para `0x400000`
  - `storage` deslocado para `0xA20000` e reduzido para `0x5E0000`
- observação operacional:
  - essa correção exige regravar tabela de partições e imagem da partição `model`; upload incremental só do app não basta

- 2026-03-28
- crash subsequente isolado no `speech_command_service`
- após `mn5q8_en` subir, as frases livres em inglês (`SLEEP`, `WAKE UP`, `LOOK AT ME`, etc.) foram rejeitadas pelo MultiNet5 com `Misspelling`
- causa:
  - o pacote `mn5q8_en` não aceita frases arbitrárias em texto puro; ele trabalha com um vocabulário/gramática compatíveis com o modelo ou com fonemas preparados
  - o tratamento de erro local ainda iterava detalhes demais do retorno e acabou derrubando o boot
- correção aplicada:
  - endurecido o fallback de erro em `speech_command_service.c` para nunca dereferenciar a lista detalhada de frases inválidas
  - o vocabulário temporário de teste foi trocado para frases stock aceitas pelo `mn5q8_en`:
    - `TURN OFF THE TV` -> `SLEEP`
    - `TURN ON THE TV` -> `WAKE`
    - `HIGHEST VOLUME` -> `VOLUME_HIGH`
    - `LOWEST VOLUME` -> `VOLUME_LOW`
    - `TURN OFF ALL THE LIGHTS` -> `GOOD_NIGHT`
    - `TURN OFF MY SOUNDBOX` -> `PRIVACY_MODE`
- próximo passo futuro:
  - se quisermos comandos ingleses naturais do NodeBot, será preciso migrar para fonemas customizados / gerar gramática compatível, não apenas strings livres

- 2026-03-28
- ajuste de integração do `mn5q8_en`
- evidência de bancada:
  - até frases stock em texto puro continuavam falhando com `Misspelling`
- conclusão:
  - no `MultiNet5`, o caminho robusto não é empurrar frases livres por `set_speech_commands()`; o pacote espera a configuração preparada/compatível do próprio sdkconfig ou fonemas customizados
- correção aplicada:
  - `speech_command_service.c` passou a usar `esp_mn_commands_update_from_sdkconfig()`
  - o serviço agora mapeia apenas um subconjunto estável dos `command_id` stock do modelo para intents do NodeBot:
    - `4` -> `PRIVACY_MODE`
    - `5` -> `VOLUME_HIGH`
    - `6` -> `VOLUME_LOW`
    - `9` -> `WAKE`
    - `10` -> `SLEEP`
    - `18` -> `GOOD_NIGHT`
- efeito esperado:
  - o boot deixa de depender de frases custom text-only
  - os testes passam a usar o vocabulário stock já empacotado pelo `esp-sr`

- 2026-03-28
- polimento incremental da frente visual/expressões
- foco da rodada:
  - abrir um pouco presets ainda “apertados” demais sem descaracterizar a expressão
  - reduzir o quanto os microefeitos do idle ainda comprimiam o olho
- ajustes em `src/models/face_params.h`:
  - `FACE_FOCUSED`: abertura aumentada e spacing levemente mais folgado
  - `FACE_FRUS_BORED`: geometria e abertura suavizadas para parecer menos colapsada
  - `FACE_UNIMP`: abertura mínima elevada e cantos inferiores menos pesados
  - `FACE_SLEEPY`: mantido sonolento, mas menos assimétrico/fechado
  - `FACE_SUSPICIOUS`: spacing menos extremo e olho menor menos colapsado
  - `FACE_SQUINT`: semicerrado mais legível
- ajustes em `src/behavior/idle_behavior.c`:
  - o guard `eyes_already_small()` ficou mais conservador (`0.50`)
  - `squint_think` passou a apertar menos a abertura base
  - `slight_smile` passou a preservar mais abertura ao curvar a parte inferior
- intenção:
  - manter personalidade visual
  - reduzir chance de leituras visuais “olho pequeno demais” em estados normais
  - seguir a estratégia de tuning pequeno e observável

- 2026-03-28
- rodada guiada pelo skill local `skills/nodebot-face`
- decisão da rodada:
  - não retunar presets de novo imediatamente
  - polir apenas a camada de rasterização para melhorar legibilidade de olhos estreitos em runtime
- ajuste em `src/face/face_renderer.cpp`:
  - altura mínima do olho aumentada de `4` para `6` px
  - simplificação progressiva da geometria para olhos estreitos:
    - `h <= 18`: reduz intensidade de curvatura
    - `h <= 14`: reduz raio de cantos
    - `h <= 10`: zera curvatura
    - `h <= 8`: zera arredondamento
- intenção:
  - evitar que estados estreitos pareçam “semi-blink” por excesso de curvatura/canto comprimido
  - melhorar leitura visual sem mexer novamente em blink/idle/presets na mesma rodada

- 2026-03-28
- rodada guiada pelo skill `nodebot-face`, focada só em `behavior_engine.c`
- alvo:
  - `TALK_THINK` ainda usava `FACE_SQUINT` puro, o que deixava o estado de processamento mais apertado do que o ideal em runtime
- ajuste aplicado:
  - `node_talking()` deixou de usar `FACE_SQUINT` no branch de thinking
  - o branch agora parte de `FACE_FOCUSED` com tuning local:
    - abertura em `0.54 / 0.54`
    - leve curvatura superior (`cv_top=2`)
    - leve apoio inferior (`cv_bot=-6`)
    - spacing `x_off=112`
    - transição `280 ms`
- intenção:
  - manter leitura de “processando/pensando”
  - reduzir aparência de semicerrado excessivo durante diálogo ativo
  - polir a seleção de baseline sem mexer novamente em preset, idle, blink e renderer na mesma rodada

- 2026-03-28
- rodada maior, mas cautelosa, guiada pela skill `nodebot-face`
- revisão estruturada em 3 itens:

1. `TALK_THINK` / `TALK_LISTEN`
- arquivo: `src/behavior/behavior_engine.c`
- `TALK_LISTEN` foi refinado a partir de `FACE_FOCUSED` para ficar mais atento e limpo:
  - abertura `0.62 / 0.62`
  - `cv_top=1`
  - `cv_bot=-7`
  - `x_off=114`
  - transição `180 ms`
- `TALK_THINK` manteve o caminho baseado em `FOCUSED`, preservando leitura de processamento sem voltar ao `SQUINT` puro

2. estados `IDLE` mais usados
- arquivo: `src/behavior/behavior_engine.c`
- ajustes locais de baseline, sem reabrir a biblioteca inteira de presets:
  - `IDLE_SLEEPY`: menos colapsado, mais legível (`0.52 / 0.70`, `x_off=110`, `760 ms`)
  - `IDLE_UNIMP`: menos estreito (`0.54 / 0.58`, `x_off=114`, `560 ms`)
  - `IDLE_HAPPY`: ficou um pouco mais aberto e espaçado (`0.42 / 0.42`, `x_off=118`, `380 ms`)
  - `IDLE_FOCUS`: abriu levemente e ganhou fundo mais estável (`0.60 / 0.60`, `cv_bot=-8`, `320 ms`)
  - `IDLE_NEUTRAL`: spacing levemente menos fechado (`x_off=110`, `440 ms`)
- intenção:
  - melhorar os estados que mais aparecem no uso real
  - reduzir a sensação de olho “apertado por default” em idle e diálogo

3. limpeza dos provisórios restantes do idle/blink
- arquivos:
  - `src/behavior/idle_behavior.c`
  - `src/face/blink_controller.cpp`
- removidos restos de modo-teste que ainda ficavam no código:
  - `s_disable_idle_blinks_test`
  - branch morto `if (false && ...)`
  - `s_disable_auto_blink_test`
- `slow_blink` voltou a seguir guardas reais em vez de desligamento por teste:
  - agora só pula quando os olhos já estão pequenos
- o threshold local de `eyes_already_small()` no blink foi alinhado para `0.50`, consistente com o idle

- intenção geral da rodada:
  - consolidar a frente facial sem dar uma guinada grande numa única camada
  - trocar provisórios por guardrails reais
  - deixar o comportamento mais coerente entre baseline, idle e blink

- 2026-03-28
- continuação cautelosa da frente de faces, ainda guiada pela skill `nodebot-face`
- foco exclusivo da rodada:
  - refinar os estados `ENGAGED`, sem voltar a mexer em blink, idle microeffects ou renderer
- arquivo:
  - `src/behavior/behavior_engine.c`
- ajustes aplicados:
  - `ENGAGED_AWE`: transição levemente mais curta e spacing um pouco mais aberto (`x_off=104`)
  - `ENGAGED_HAPPY`: abertura local `0.42 / 0.42`, spacing `118`, transição `280 ms`
  - `ENGAGED_FOCUS`: abertura `0.60 / 0.60`, `cv_bot=-8`, transições mais curtas (`280/260 ms`)
  - `ENGAGED_NEUT`: neutral mais rápido e menos “fechado por default” (`x_off=110`, `340 ms`)
- intenção:
  - deixar o bloco social/engajado mais coerente com os refinamentos anteriores de `TALK` e `IDLE`
  - reduzir rigidez visual após wake, atenção e presença de pessoa

- 2026-03-28
- diagnóstico do caminho `wake + comando` quando havia beep/expressão, mas a action não consolidava
- causa isolada:
  - a intent `SLEEP` no `offline_command_service` só reduzia `energy/attention`
  - a FSM ainda precisava atravessar `TALKING -> ENGAGED -> IDLE -> SLEEP`
  - se `attention`/contexto continuassem altos, o robô não entrava em sono efetivo
- correção aplicada:
  - `behavior_engine` ganhou requests explícitos:
    - `behavior_engine_request_sleep()`
    - `behavior_engine_request_wake()`
  - `offline_command_service` agora usa esses requests nas intents `SLEEP/GOOD_NIGHT` e `WAKE`
- melhoria de diagnóstico:
  - `speech_command_service` passou a logar:
    - `recognized cmd_id=<id> intent=<intent> prob=<p>`
  - se o comando reconhecido não tiver mapping útil, agora isso aparece explicitamente no log
- efeito esperado:
  - `wake + command` deixa de parecer “entendi mas não fiz nada”
  - a bancada consegue distinguir com clareza:
    - não reconheceu
    - reconheceu sem mapping
    - reconheceu e executou

- 2026-03-28
- ajuste de timing no `speech_command_service` para testes pós-wake
- evidência de bancada:
  - o `SPCH_CMD` armava junto com o wake
  - a própria lógica pulava feed enquanto o bip/feedback estava tocando
  - na prática, parte da janela útil era desperdiçada antes do usuário começar o comando
- correção aplicada em `src/svc_audio/speech_command_service.c`:
  - `SPEECH_CMD_WINDOW_MS`: `2600 -> 4200`
  - `SPEECH_CMD_TIMEOUT_MS`: `1800 -> 2600`
  - se houver playback ativo após o wake:
    - o serviço agora espera o fim do áudio
    - reseta o MultiNet
    - reinicia a janela útil de escuta após o bip
- logs novos de bancada:
  - `waiting playback end before command listen`
  - `command listen start after playback`
- intenção:
  - fazer `jarvis` + pausa + comando curto funcionar de forma mais realista
  - separar melhor a wake phrase do comando útil

- 2026-03-28
- isolamento da bancada de wake em `jarvis`
- evidência:
  - o boot mostrava dois modelos de wake ativos ao mesmo tempo:
    - `wn9_hiesp`
    - `wn9_jarvis_tts`
  - isso deixava a avaliação de sensibilidade do `jarvis` contaminada por uma configuração multi-wake
- correção aplicada:
  - `sdkconfig`: `CONFIG_SR_WN_WN9_HIESP` desabilitado
  - `CONFIG_SR_WN_WN9_JARVIS_TTS` mantido como único wake word ativo
- intenção:
  - medir o comportamento real do `jarvis` sem concorrência de outra wake phrase
  - só depois decidir se ainda precisa mexer em `DET_MODE_95`

- 2026-03-28
- correção de ordem no `speech_command_service` após análise de logs de bancada
- sintoma observado:
  - depois de `PROCESSING -> IDLE`, o serviço ainda podia entrar em:
    - `waiting playback end before command listen`
    - `command listen start after playback`
  - e só então se desarmar por `dialogue_idle`
- causa:
  - `audio_feedback_is_playing()` era checado antes de verificar se o diálogo já estava em `IDLE`
- correção aplicada:
  - `speech_command_service_feed()` agora verifica `DIALOGUE_STATE_IDLE` antes do gating por playback
- efeito esperado:
  - a janela de command listen morre imediatamente quando o diálogo encerra
  - os logs deixam de sugerir uma falsa “segunda escuta” depois do timeout

- 2026-03-28
- contenção defensiva no driver `ws2812` após enxurrada de `rmt_tx_wait_all_done(...): flush timeout`
- evidência de bancada:
  - durante `wake -> TALK_LISTEN -> command window`, o serial passou a repetir dezenas de timeouts do RMT
  - o wait do LED estava sendo chamado com timeout de `100 ms`, inclusive a partir da `ESP_TIMER_TASK`
  - isso podia contaminar o timing do pós-wake e poluir o diagnóstico do reconhecimento
- correção aplicada em `src/drivers/led/ws2812_driver.c`:
  - timeout de espera do flush reduzido para `5 ms`
  - timeout do RMT deixa de ficar gerando erro em cascata no driver/base
  - em caso de timeout:
    - o driver registra um warning com throttle
    - reabilita o canal RMT
    - descarta apenas aquele flush atrasado
- intenção:
  - impedir que a animação de LED atrapalhe a janela de comando pós-wake
  - manter o comportamento visual degradando com graça, em vez de travar ou spammar erro

- 2026-03-28
- segunda contenção no `ws2812` após persistência do mesmo erro-base de `rmt_tx_wait_all_done`
- conclusão:
  - o wrapper anterior ainda não bastava porque o erro vinha do próprio caminho bloqueante do flush no driver RMT
  - para LED de status, sincronização rígida de flush é pior do que envio opportunistic
- correção aplicada em `src/drivers/led/ws2812_driver.c`:
  - remoção do `rmt_tx_wait_all_done()` do caminho crítico
  - rate limit de flush para ~`20 ms` entre frames
  - manutenção de warning com throttle apenas para falha real de `rmt_transmit()`
- intenção:
  - eliminar a fonte de `flush timeout` repetitivo
  - preservar LED como best-effort sem competir com wake, diálogo e reconhecimento

- 2026-03-28
- última rodada curta de tuning do `MultiNet` pós-wake após logs limpos seguirem terminando em `mn_timeout`
- diagnóstico:
  - com wake, playback gating e LED já estabilizados, o gargalo remanescente passou a ser a rigidez do reconhecimento
  - o sistema abria a janela corretamente, mas não produzia `recognized cmd_id=...`
- ajuste aplicado em `src/svc_audio/speech_command_service.c`:
  - `SPEECH_CMD_WINDOW_MS`: `4200 -> 5600`
  - `SPEECH_CMD_TIMEOUT_MS`: `2600 -> 3400`
  - `SPEECH_CMD_DET_THRESHOLD`: `0.60 -> 0.52`
- intenção:
  - dar mais tempo útil ao comando curto após o wake
  - reduzir um pouco a rigidez do `mn5q8_en` sem afrouxar demais a bancada

- 2026-03-28
- cleanup do serial para manter a bancada focada em `wake`
- ajuste aplicado:
  - diagnósticos temporários de face `SMALL_EYES` / `FACE_RECOVER` foram rebaixados para `DEBUG`
  - isso remove ruído visual do serial sem desmontar os guardrails internos
- intenção:
  - deixar o monitor legível para investigar apenas `wake`, `LISTENING`, `command window` e reconhecimento

Limpeza de logs para monitor focado no wake:
- `src/behavior/idle_behavior.c`
  - logs de `tier2` e `skip ... eyes already small` rebaixados para `DEBUG`
- `src/behavior/behavior_engine.c`
  - `FACE -> ...`, `SMALL_EYES ...` e `FACE_RECOVER ...` rebaixados para `DEBUG`
- `src/face/face_engine.cpp`
  - `FACE: SMALL_EYES ...` rebaixado para `DEBUG`
- `src/svc_audio/dialogue_state_service.c`
  - mudanças de estado e timeouts rebaixados para `DEBUG`
- `src/services/led_router/led_router.c`
  - `state -> ...` rebaixado para `DEBUG`

Motivação:
- o monitor estava poluído demais para uso normal
- o objetivo foi deixar `WAKE: WAKE WORD ...` visível em `INFO` e preservar erros reais, mantendo o resto disponível apenas para debug quando necessário
- 2026-03-28
- cleanup final do serial para foco exclusivo em wake
- ajuste aplicado:
  - logs de runtime de BENG, FACE, DIALOGUE e LED_ROUTER foram rebaixados para DEBUG
  - a trilha normal de bancada fica centrada em WAKE e SPCH_CMD
- intenção:
  - remover ruído visual do monitor serial
  - deixar o diagnóstico concentrado apenas na frente de wake/comando

- 2026-03-28
- limpeza da camada de comandos para reduzir conflito entre `texto offline` e `wake + mn5q8_en`
- ajuste aplicado:
  - `offline_command_service.c` deixou de aceitar aliases em inglês que sugeriam suporte de fala livre via wake
  - o matcher textual local ficou centrado nas frases em português do NodeBot
  - `speech_command_service.c` deixou de imprimir o dump completo de comandos ativos do MultiNet no boot
  - o subconjunto realmente mapeado do `mn5q8_en` ficou documentado no próprio código
- intenção:
  - evitar ambiguidade entre “funciona por texto/serial” e “funciona falado após wake”
  - deixar clara a diferença entre intents do NodeBot e vocabulário stock do `mn5q8_en`

- 2026-03-28
- retomada cautelosa da frente de face com ajuste isolado no `blink_controller`
- evidência de bancada:
  - mesmo após estabilizações anteriores, ainda apareciam alvos de blink em `0.08 / 0.02`
  - isso seguia alimentando leitura de `small-eyes` e sensação de alvo “quase colado” em estados neutros
- correção aplicada em `src/face/blink_controller.cpp`:
  - fechamento do blink ficou menos agressivo
  - `KF1` passou a usar escala `0.24`
  - `KF2` passou a usar escala `0.14`
  - ambos agora respeitam um piso de abertura `0.14`
- intenção:
  - manter leitura clara de piscada no painel
  - evitar que o target do FaceEngine caia repetidamente em valores muito próximos de zero

- 2026-03-28
- polimento cauteloso dos baselines neutros após a rodada do blink
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `IDLE_NEUTRAL` ganhou abertura local `0.96 / 0.96` e spacing `112`
  - `ENGAGED_NEUT` ganhou abertura local `0.94 / 0.94` e spacing `112`
  - `ENGAGED_AWE` ficou um pouco menos estreito no baseline (`0.96 / 0.96`, `x_off=108`)
- intenção:
  - acompanhar o blink menos agressivo com baselines neutros mais estáveis em runtime
  - reduzir a chance de a face voltar a parecer semi-fechada logo após transições comuns

- 2026-03-28
- polimento cauteloso dos microefeitos do `idle_behavior`
- correção aplicada em `src/behavior/idle_behavior.c`:
  - `squint_think` ficou menos apertado (`0.90x`, piso `0.30`)
  - `slow_blink` do idle ficou menos fundo (`0.52x`, piso `0.18`)
  - `slight_smile` preserva mais abertura (`0.96x`, piso `0.24`)
- intenção:
  - evitar que microcomportamentos comuns do idle levem o olho de volta para a zona visual de semi-fechado
  - acompanhar as rodadas anteriores de blink e baselines neutros sem reabrir presets/renderer

- 2026-03-28
- reforço cauteloso do recovery em runtime no `behavior_engine`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - o `FACE_RECOVER` deixou de reagir só a casos extremos (`<= 0.12`)
  - agora o baseline pode ser reaplicado quando um target não-sleep cair para `<= 0.24 / 0.24`
- intenção:
  - recuperar mais cedo estados neutros/engaged quando o target ainda estiver baixo demais para leitura normal
  - fechar a sequência de estabilização sem reabrir presets, renderer e blink na mesma rodada

- 2026-03-28
- guardrail final no `behavior_engine` para o face command composto
- correção aplicada em `src/behavior/behavior_engine.c`:
  - o `final_cmd.params` agora passa por uma normalização leve em runtime fora de estados sleep-like
  - piso aplicado:
    - abertura mínima `0.34 / 0.34`
    - spacing mínimo `108`
- intenção:
  - impedir que a composição final ainda entregue ao `FaceEngine` um alvo pequeno demais para leitura normal
  - acrescentar uma proteção tardia sem reabrir blink, idle, presets e renderer

- 2026-03-28
- clamp cauteloso no `face_engine` durante interpolação
- correção aplicada em `src/face/face_engine.cpp`:
  - quando o destino da transição ainda estiver em faixa muito baixa (`<= 0.24 / 0.24`)
  - e a transição não for instantânea, o estado interpolado passa por um piso leve em runtime:
    - abertura mínima `0.34 / 0.34`
    - spacing mínimo `108`
- intenção:
  - evitar que o render loop continue perseguindo por muitos frames um destino pequeno demais para estados neutros/engaged
  - fechar a cadeia de estabilização com um guardrail local da própria interpolação

- 2026-03-28
- cleanup controlado dos diagnósticos provisórios da frente de face
- ajuste aplicado:
  - remoção dos logs/auxiliares temporários de `SMALL_EYES` em:
    - `src/behavior/behavior_engine.c`
    - `src/face/face_engine.cpp`
  - os guardrails de estabilização permaneceram ativos:
    - blink menos agressivo
    - baselines neutros mais abertos
    - microefeitos do idle mais conservadores
    - recovery mais cedo
    - pisos de runtime no `behavior_engine` e no `face_engine`
- intenção:
  - reduzir complexidade e sujeira de código após a investigação
  - manter apenas as proteções que de fato ajudam a estabilidade visual

- 2026-03-28
- polimento cauteloso da família positiva/social no `behavior_engine`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `ENGAGED_GLEE` e `IDLE_GLEE` ganharam abertura local `0.40 / 0.40` e spacing `116`
  - `ENGAGED_HAPPY` e `IDLE_HAPPY` passaram para `0.48 / 0.48` com spacing `116`
  - `IDLE_AWE` ganhou override leve para `0.96 / 0.96` com `x_off=106`
- intenção:
  - deixar felicidade e sociabilidade mais legíveis em runtime sem leitura de olho apertado
  - seguir o polimento em uma única camada, sem reabrir blink, idle, presets ou renderer

- 2026-03-28
- polimento cauteloso da família triste/tensa no `behavior_engine`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `SYSTEM_ALERT` (`FACE_WORRIED`) ganhou abertura local `0.92 / 0.92` com spacing `110`
  - `ENGAGED_SAD` (`FACE_SAD_UP`) ganhou override `0.82 / 0.82` com `x_off=112`
  - `IDLE_SAD_D` (`FACE_SAD_DOWN`) passou para `0.74 / 0.74` com `x_off=112`
  - `IDLE_SAD_U` (`FACE_SAD_UP`) passou para `0.82 / 0.82` com `x_off=112`
- intenção:
  - preservar leitura emocional de tristeza/preocupação sem deixar a família pesada demais em runtime
  - manter o polimento concentrado em uma única camada antes de reavaliar em hardware

- 2026-03-28
- polimento cauteloso da família de atenção no `behavior_engine`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `TALK_LISTEN` ficou mais aberto e atento (`0.66 / 0.66`, `x_off=116`, `cv_bot=-6`, transição `160ms`)
  - `TALK_THINK` ficou um pouco menos apertado (`0.58 / 0.58`, `x_off=114`, transição `260ms`)
  - `ENGAGED_FOCUS` e `IDLE_FOCUS` passaram para `0.64 / 0.64`, `x_off=114` e `cv_bot=-7`
  - `ENGAGED_AWE` ganhou spacing ligeiramente mais aberto (`x_off=106`)
- intenção:
  - deixar estados de atenção e diálogo mais legíveis em runtime sem recair em leitura de semi-fechado
  - seguir o polimento ainda em uma única camada antes de voltar a validar em hardware

- 2026-03-28
- rodada curta de consistência fina para neutros e alerta no `behavior_engine`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `SYSTEM_ALERT` (`FACE_WORRIED`) ficou um pouco mais aberto e responsivo (`0.94 / 0.94`, `x_off=111`, transição `360ms`)
  - `ENGAGED_NEUT` foi alinhado para `0.95 / 0.95` com transição `320ms`
  - `IDLE_NEUTRAL` foi alinhado para `0.97 / 0.97` com transição `400ms`
- intenção:
  - reduzir saltos de leitura entre neutral, engaged e alerta leve após o polimento das outras famílias
  - fechar esta sequência de ajustes ainda em uma única camada antes de voltar para validação em hardware

- 2026-03-28
- micro-rodada de consistência para expressão de evento curto no `behavior_engine`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `MUSIC` (`FACE_HAPPY`) foi alinhado com a família feliz recente
  - passou a usar abertura `0.50 / 0.50`, spacing `116` e transição `320ms`
- intenção:
  - evitar que a expressão de música fique mais apertada ou mais lenta que os demais estados felizes já polidos
  - manter a rodada pequena e observável sem abrir outra camada do pipeline

- 2026-03-28
- cleanup cauteloso de consistência no `behavior_engine`
- ajuste aplicado em `src/behavior/behavior_engine.c`:
  - criação de helpers locais para consolidar overrides repetidos de famílias afinadas:
    - `make_tuned_happy(...)`
    - `make_tuned_focus(...)`
    - `make_tuned_neutral(...)`
  - estados já polidos (`HAPPY`, `GLEE`, `FOCUS`, `NEUTRAL`, `SAD`, `SYSTEM_ALERT`, `MUSIC`) passaram a reutilizar esses helpers sem mudar seus valores visuais
- intenção:
  - reduzir duplicação e risco de drift entre estados já afinados
  - deixar a próxima rodada de face mais segura sem abrir outra camada do pipeline

- 2026-03-28
- cleanup final de organização para a família de diálogo no `behavior_engine`
- ajuste aplicado em `src/behavior/behavior_engine.c`:
  - criação de helper local `make_tuned_talk_focus(...)`
  - `TALK_LISTEN` e `TALK_THINK` passaram a reutilizar o helper sem mudar os valores visuais já afinados
- intenção:
  - manter a família de diálogo consistente com o cleanup já feito para `happy`, `focus` e `neutral`
  - encerrar esta sequência de trabalho com código mais limpo antes de nova validação em hardware

- 2026-03-28
- suporte de bancada guiada para a frente de faces via serial
- ajuste aplicado em `src/svc_audio/offline_command_service.c`:
  - adição do comando `facebench <mode>` no serial local
  - modos disponíveis:
    - `idle`
    - `engaged`
    - `listen`
    - `think`
    - `happy`
    - `glee`
    - `sad`
    - `alert`
    - `music`
    - `sleepy`
    - `unimp`
    - `sleep`
- documentação atualizada em `docs/Face_Bench_Checklist.md`
- intenção:
  - permitir validação de face mais controlada em bancada sem depender só de gatilhos orgânicos
  - acelerar comparação visual entre estados já polidos

- 2026-03-28
- ajuste corretivo para distinguir `SYSTEM_ALERT` da família `sad`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `SYSTEM_ALERT` deixou de usar só abertura parecida com `sad`
  - passou para `0.96 / 0.96`, `x_off=108`, transição `280ms`
  - ganhou geometria mais própria de alerta/preocupação:
    - `cv_top=2`
    - `cv_bot=-2`
    - `y_l=y_r=2`
- intenção:
  - afastar visualmente `alert` de `sad` na bancada
  - dar leitura mais tensa e responsiva de aviso leve sem reabrir outras camadas

- 2026-03-28
- overlay visual de alerta piscando na frente de faces
- correção aplicada:
  - `src/face/face_engine.h`
  - `src/face/face_engine.hpp`
  - `src/face/face_engine.cpp`
  - `src/behavior/behavior_engine.c`
- ajuste:
  - adição de um flag explícito de overlay de alerta no `FaceEngine`
  - `SYSTEM_ALERT` agora liga esse overlay a partir do `behavior_engine`
  - o render desenha um pequeno triângulo de alerta piscando acima dos olhos
- intenção:
  - diferenciar `alert` da família `sad` sem alterar a expressão base toda vez
  - acrescentar sinal visual claro de aviso leve com impacto localizado e reversível

- 2026-03-28
- tentativa experimental de overlay visual de música foi revertida
- ajuste aplicado:
  - remoção do overlay de fones que havia sido ligado ao estado `MUSIC`
- motivo:
  - o resultado visual ficou perto da ideia, mas ainda não bom o suficiente para manter no firmware
- intenção:
  - preservar a frente de faces limpa enquanto a direção visual de `music` não estiver mais madura

- 2026-03-28
- overlay visual de sono para o estado `SLEEP`
- correção aplicada:
  - `src/face/face_engine.h`
  - `src/face/face_engine.hpp`
  - `src/face/face_engine.cpp`
  - `src/behavior/behavior_engine.c`
- ajuste:
  - adição de um flag explícito de overlay de sono no `FaceEngine`
  - o estado `SLEEP` agora liga esse overlay automaticamente a partir do `behavior_engine`
  - o render desenha uma animação leve de `Z` flutuando acima da face
- intenção:
  - diferenciar `sleep` de `sleepy` com um sinal visual simples e claro
  - acrescentar leitura de “dormindo” sem mexer em servos nem reabrir a base facial

- 2026-03-28
- correção visual do overlay de `alert` para combinar com a tela
- ajuste aplicado em `src/face/face_engine.cpp`:
  - remoção do triângulo preenchido que destoava do restante da face
  - overlay passou a usar a mesma cor da face (`FACE_EYE_COLOR`)
  - o símbolo foi reposicionado para o canto superior direito, fora da área dos olhos
  - desenho agora é mais leve, em contorno, sem fundo chapado
- intenção:
  - alinhar o `alert` com a linguagem visual do restante da tela
  - evitar sobreposição no olho e sensação de elemento “colado por cima”

- 2026-03-28
- correção visual do overlay de `sleep` para combinar com a face
- ajuste aplicado em `src/face/face_engine.cpp`:
  - remoção do `Z` baseado em fonte/caractere
  - overlay passou a desenhar `Z` por linhas, sem caixa de fundo
  - uso da mesma cor da face (`FACE_EYE_COLOR`)
  - reposicionamento do efeito para o canto superior direito, fora da área dos olhos
- intenção:
  - alinhar a animação de sono com a linguagem visual da tela
  - evitar aparência de texto solto sobre a face

- 2026-03-28
- separação explícita entre `sleepy` e `sleep` na biblioteca facial
- correção aplicada:
  - `src/models/face_params.h`
  - `src/behavior/behavior_engine.c`
- ajuste:
  - criação de `FACE_SLEEP` como preset próprio para dormir de fato
  - `node_sleep()` deixou de reutilizar `FACE_SLEEPY` e passou a usar `FACE_SLEEP`
- intenção:
  - manter `sleepy` como estado de sonolência ainda acordado
  - deixar `sleep` com desenho base próprio, além do overlay de sono

- 2026-03-28
- endurecimento do `facebench sleep` apenas para bancada
- correção aplicada em `src/svc_audio/offline_command_service.c`:
  - o modo `facebench sleep` agora limpa melhor contexto concorrente (`person_present`, `music_detected`, `social_need`)
  - mantém `energy`, `attention` e `mood_arousal` em faixa compatível com `SLEEP`
- intenção:
  - evitar retorno prematuro para `IDLE` durante observação do estado de sono
  - melhorar a confiabilidade do teste sem alterar a FSM real do robô

- 2026-03-28
- correção de requests conflitantes entre `sleep` e `wake`
- correção aplicada em `src/behavior/behavior_engine.c`:
  - `behavior_engine_request_sleep()` agora limpa `s_wake_req`
  - `behavior_engine_request_wake()` agora limpa `s_sleep_req`
- intenção:
  - evitar que um request antigo empurre a FSM para fora do estado recém-solicitado no tick seguinte
  - corrigir especialmente o caso de bancada em que `sleep` acabava virando `sleepy` logo depois

- 2026-03-29
- fechamento da fase de polimento e validação da frente de faces
- resultado:
  - validação de bancada concluída
  - famílias principais verificadas em uso real:
    - `idle`
    - `engaged`
    - `talk`
    - `sad`
    - `alert`
    - `sleepy`
    - `sleep`
  - `facebench` permaneceu como apoio útil de bancada
- observações de fechamento:
  - `alert` ficou com identidade própria via overlay leve
  - `sleep` ficou separado de `sleepy` por preset base e overlay
  - tentativa de overlay de `music` foi revertida e fica como backlog visual futuro
- intenção:
  - marcar esta fase como concluída
  - deixar o histórico explícito antes de criar checkpoint de código
