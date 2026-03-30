# E21 - Vetor de Estado Interno

- Status: 🔲 Não iniciada
- Complexidade: Baixa
- Grupo: Gaze+Idle
- HW Real: SIM
- Notas: state_vector_t com 7 dimensões. Ver pág. 3 (Behavioral State Machine) seção 3.4 da Análise de Comportamento.
- Prioridade: P1
- Risco: BAIXO
- Depende de: E20 (GazeService)

## Vetor de Estado Interno

Complexidade: Baixa
Depende de: E20 (GazeService)
Grupo: Gaze+Idle
HW Real: SIM
ID: E21
Notas: state_vector_t com 7 dimensões. Ver pág. 3 (Behavioral State Machine) seção 3.4 da Análise de Comportamento.
Prioridade: P1
Risco: BAIXO
Status: 🔲 Não iniciada

> 📍 **E21** · Grupo: Gaze+Idle · Prioridade: P1 · Depende de: E20 (GazeService)
> 

---

## Objetivo

Definir e implementar o `state_vector_t` — o vetor de estado interno do robô. É ele que determina como o robô "se sente" em cada momento e alimenta automaticamente a expressão facial, o comportamento idle e as reações.

---

## O que é o StateVector

O StateVector é um conjunto de variáveis contínuas que representam o estado interno do robô. Não é um FSM — é um **vetor dimensional** que evolui continuamente ao longo do tempo. O BehaviorEngine (E33) lê esse vetor e decide qual expressão e comportamento usar.

```
energy      0.0 ──────────────────── 1.0
            (exausto)              (alerta)

mood_valence -1.0 ─────── 0.0 ─────── +1.0
             (triste)   (neutro)    (feliz)

mood_arousal  0.0 ──────────────────── 1.0
              (calmo)              (agitado)

social_need   0.0 ──────────────────── 1.0
              (satisfeito)         (isolado)

attention     0.0 ──────────────────── 1.0
              (disperso)            (focado)

comfort       0.0 ──────────────────── 1.0
              (desconfortável)       (bem)

affinity      0.0 ──────────────────── 1.0
              (desconhecido)      (próximo)
```

---

## O que implementar

```c
// src/models/state_vector.h

typedef struct {
    // --- Dimensões contínuas ---
    float energy;          // 0.0-1.0 | tau=4h | afeta eyelid e blink rate
    float mood_valence;    // -1.0 a +1.0 | tau=6h | afeta highlight, squint e expressão base
    float mood_arousal;    // 0.0-1.0 | tau=30min | afeta velocidade de movimentos
    float social_need;     // 0.0-1.0 | sobe com isolamento, cai com interação
    float attention;       // 0.0-1.0 | tau=10min | foco no usuário atual
    float comfort;         // 0.0-1.0 | afetado por temperatura e bateria
    float affinity;        // 0.0-1.0 | permanente | afeto acumulado pelo usuário

    // --- Contadores de interação ---
    uint32_t last_interaction_ms;   // timestamp da última interação
    uint32_t session_interactions;  // interações na sessão atual
    uint32_t total_interactions;    // lifetime
    uint32_t time_awake_ms;         // acordado desde quando

    // --- Flags de contexto ---
    bool music_detected;
    bool person_present;
    bool being_touched;
    bool charging;
    float battery_pct;     // 0.0-100.0
} state_vector_t;

// Instância global (singleton)
extern state_vector_t g_state;

// API
void state_vector_init(void);
void state_vector_tick(uint32_t now_ms);  // chamado a cada 1s pelo BehaviorLoop
void state_vector_on_interaction(void);   // registrar interação
void state_vector_on_touch(bool rough);
void state_vector_on_pet(void);           // carinho — incrementa affinity
void state_vector_save(void);             // persistir affinity no NVS
void state_vector_load(void);             // carregar affinity do NVS
```

---

## Como cada dimensão evolui

### energy

```c
// Decai naturalmente com o tempo (robô cansa)
// Recupera durante sono
// Modificado pelo CircadianService (E35) por horário do dia
void update_energy(uint32_t now_ms) {
    float hours_awake = g_state.time_awake_ms / 3600000.0f;
    float natural_decay = hours_awake * 0.04f;  // -4% por hora acordado
    float target = 0.9f - natural_decay;
    target = fmaxf(0.1f, fminf(1.0f, target));
    // Suavizar com filtro passa-baixa (tau=4h)
    float alpha = 1.0f - expf(-1.0f / (4.0f * 3600.0f));  // tau=4h, tick=1s
    g_state.energy += alpha * (target - g_state.energy);
}
```

### mood_valence

```c
// Sobe com interações positivas, toque gentil, música
// Cai com isolamento, toque brusco, bateria baixa
// Retorna gradualmente para 0 (neutro) com tau=6h
void update_valence(void) {
    float target = 0.0f;  // neutro como atrator base
    float alpha = 1.0f - expf(-1.0f / (6.0f * 3600.0f));  // tau=6h
    g_state.mood_valence += alpha * (target - g_state.mood_valence);
    g_state.mood_valence = fmaxf(-1.0f, fminf(1.0f, g_state.mood_valence));
}
```

### social_need

```c
// Sobe a cada segundo sem interação (robô quer atenção)
// Zera ao detectar presença + interação
void update_social_need(uint32_t now_ms) {
    uint32_t idle_ms = now_ms - g_state.last_interaction_ms;
    float idle_hours = idle_ms / 3600000.0f;
    // Sobe linearmente até 1.0 em ~2h sem interação
    g_state.social_need = fminf(1.0f, idle_hours / 2.0f);
}
```

### affinity (persistente)

```c
// Incrementos por tipo de interação
void state_vector_on_pet(void) {
    g_state.affinity = fminf(1.0f, g_state.affinity + 0.0005f);
    // Salvar no NVS a cada 0.01 de mudança
}

void state_vector_on_interaction(void) {
    g_state.affinity = fminf(1.0f, g_state.affinity + 0.002f);
    g_state.last_interaction_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    g_state.session_interactions++;
    g_state.total_interactions++;
    g_state.social_need = fmaxf(0.0f, g_state.social_need - 0.15f);
    g_state.mood_valence = fminf(1.0f, g_state.mood_valence + 0.08f);
    g_state.mood_arousal = fminf(1.0f, g_state.mood_arousal + 0.20f);
}
```

---

## Como o StateVector afeta a face (resumo)

| Dimensão | Valor baixo | Valor alto | Afeta |
| --- | --- | --- | --- |
| `energy` | eyelid baixo, blink lento | eyelid alto, blink normal | `open_l/r`, blink rate |
| `mood_valence` | SAD_DOWN ou expressão triste | HAPPY ou GLEE | expressão base |
| `mood_arousal` | movimentos lentos | movimentos rápidos | `transition_ms` |
| `social_need` | indiferente ao usuário | busca contato visual | gaze bias |
| `attention` | gaze idle aleatório | gaze fixo no usuário | gaze target |
| `affinity` | expressões contidas | expressões ricas, LOVE | escala de expressividade |

---

## Critérios de pronto

- [x]  `state_vector_t` definido com 7 dimensões + contadores + flags
- [x]  `state_vector_tick()` rodando a cada 1s via BehaviorLoop
- [x]  `energy` decaindo naturalmente e recuperando no sono
- [x]  `social_need` subindo sem interação, zerando com presença
- [x]  `affinity` persistida no NVS (sobrevive ao reboot)
- [x]  `state_vector_on_interaction()` atualiza valence + arousal + social_need
- [x]  Log a cada 60s: `[STATE] energy=X.XX valence=X.XX arousal=X.XX social=X.XX`

---

## ▶ Prompt Principal

```jsx
Projeto: robô desktop ESP32-S3, FreeRTOS, ESP-IDF v5.x.
Sem malloc em hot paths. EventBus entre módulos.
E21 — Vetor de Estado Interno (state_vector_t).

Tarefa: implementar state_vector.h/.c

Struct com 7 dimensões contínuas:
  float energy;        // 0.0-1.0, tau=4h
  float mood_valence;  // -1.0 a +1.0, tau=6h
  float mood_arousal;  // 0.0-1.0, tau=30min
  float social_need;   // 0.0-1.0, sobe sem interação
  float attention;     // 0.0-1.0, tau=10min
  float comfort;       // 0.0-1.0, bateria + temperatura
  float affinity;      // 0.0-1.0, permanente, persistida no NVS

Contadores: last_interaction_ms, session_interactions, total_interactions, time_awake_ms
Flags: music_detected, person_present, being_touched, charging, battery_pct

Funções:
  void state_vector_init(void)            — carregar affinity do NVS, defaults
  void state_vector_tick(uint32_t now_ms) — chamado a cada 1s
    energy: decai 4%/hora, filtro passa-baixa tau=4h
    mood_valence: retorna para 0 com tau=6h
    mood_arousal: retorna para 0 com tau=30min
    social_need: sobe linearmente, 0→1.0 em 2h sem interação
    attention: decai com tau=10min sem foco
    time_awake_ms: incrementar 1000ms por tick
  void state_vector_on_interaction(void)  — registrar interação
  void state_vector_on_touch(bool rough)  — toque
  void state_vector_on_pet(void)          — carinho
  void state_vector_save(void)            — salvar affinity no NVS
  void state_vector_load(void)            — carregar affinity do NVS
  void state_vector_log(void)             — logar estado atual

Log a cada 60s: [STATE] energy=X.XX valence=X.XX arousal=X.XX social=X.XX affinity=X.XX

Saída: src/models/state_vector.h + src/behavior/state_vector.c
```

## ◎ Prompt de Revisão

```
StateVector da E21.
Verificar: (1) filtros passa-baixa corretos (alpha = 1 - exp(-dt/tau))? (2) affinity persistida no NVS (não só em RAM)? (3) social_need sobe sem interação e cai com state_vector_on_interaction()? (4) energy decai com tempo acordado? (5) log a cada 60s?
Listar problemas.
```

## ✎ Prompt de Correção

```
StateVector com problema: [sintoma]
Contexto: E21, ESP32-S3, FreeRTOS.
Verificar: cálculo do filtro passa-baixa, persistência NVS, unidades de tempo (ms vs s).
Causa + fix.
```

## → Prompt de Continuidade

```jsx
E21 concluída. state_vector_t implementado com 7 dimensões, tick a cada 1s, affinity persistida.
Próxima: E22 (IdleLife — comportamentos automáticos em repouso usando o StateVector).
Mostre como usar energy e social_need do StateVector para selecionar e disparar comportamentos idle em 3 tiers (micro/ocasional/raro).
```


