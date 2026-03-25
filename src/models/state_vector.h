#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * state_vector_t — vetor de estado interno do robô (E21).
 *
 * Dimensões contínuas com decaimentos independentes:
 *   energy        tau=4h, linear drain 4%/h
 *   mood_valence  tau=6h, retorna a 0
 *   mood_arousal  tau=30min, retorna a 0
 *   social_need   sobe 0→1 em 2h sem interação
 *   attention     tau=10min, decai sem foco
 *   comfort       afetado por bateria e temperatura
 *   affinity      permanente, persistida no NVS
 */
typedef struct {
    /* ── Dimensões contínuas ─────────────────────────────────────────── */
    float energy;          /* [0,1]   tau=4h  | afeta eyelid, blink rate  */
    float mood_valence;    /* [-1,+1] tau=6h  | afeta expressão base      */
    float mood_arousal;    /* [0,1]   tau=30m | afeta velocidade de movimentos */
    float social_need;     /* [0,1]   sobe com isolamento, cai com interação  */
    float attention;       /* [0,1]   tau=10m | foco no usuário atual      */
    float comfort;         /* [0,1]   bateria + temperatura                */
    float affinity;        /* [0,1]   permanente | afeto acumulado         */

    /* ── Contadores ──────────────────────────────────────────────────── */
    uint32_t last_interaction_ms;   /* timestamp da última interação (ms)  */
    uint32_t session_interactions;  /* interações na sessão atual          */
    uint32_t total_interactions;    /* lifetime (NVS futuro)               */
    uint32_t time_awake_ms;         /* ms desde o boot                     */

    /* ── Flags de contexto ───────────────────────────────────────────── */
    bool  music_detected;
    bool  person_present;
    bool  being_touched;
    bool  charging;
    float battery_pct;              /* 0.0–100.0                           */
} state_vector_t;

/* Instância global (singleton) */
extern state_vector_t g_state;

/* ── API ─────────────────────────────────────────────────────────────── */

/* Carrega affinity do NVS e aplica defaults; chame no boot. */
void state_vector_init(void);

/* Chamado a cada 1 s pelo BehaviorLoop. Aplica decaimentos e log 60 s. */
void state_vector_tick(uint32_t now_ms);

/* Registra qualquer interação com o usuário. */
void state_vector_on_interaction(void);

/* Toque: rough=true → áspero/susto; false → gentil. */
void state_vector_on_touch(bool rough);

/* Carinho longo — incrementa affinity e melhora valência. */
void state_vector_on_pet(void);

/* Persiste affinity no NVS. */
void state_vector_save(void);

/* Carrega affinity do NVS (chamado em init). */
void state_vector_load(void);

#ifdef __cplusplus
}
#endif
