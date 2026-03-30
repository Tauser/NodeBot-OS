#include "state_vector.h"
#include "blink_controller.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "STATE";

/* ── Instância global ────────────────────────────────────────────────── */
state_vector_t g_state;

/* ── Constantes de decaimento (por segundo, dt=1s) ───────────────────── */
/*
 * Fórmula: decay = 1 - 1/tau_s  (aproximação válida para tau >> dt=1s)
 * Equivale a: exp(-dt/tau) para dt << tau
 */
#define DECAY_6H      0.99995370f   /* tau = 21600 s */
#define DECAY_30MIN   0.99944463f   /* tau =  1800 s */
#define DECAY_10MIN   0.99833611f   /* tau =   600 s */

/* Energy: drain linear 4%/hora (0.04 / 3600 por tick de 1 s) */
#define ENERGY_DRAIN_S   (0.04f / 3600.0f)

/* Social need: 0 → 1.0 em 2 h sem interação (1/7200 por tick de 1 s) */
#define SOCIAL_RISE_S    (1.0f / 7200.0f)

/* NVS */
#define NVS_NS           "nodebot"
#define NVS_KEY_AFFINITY "affinity"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* ── state_vector_load ───────────────────────────────────────────────── */
void state_vector_load(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open falhou (0x%x) — affinity=0", err);
        return;
    }
    uint32_t bits = 0;
    if (nvs_get_u32(h, NVS_KEY_AFFINITY, &bits) == ESP_OK) {
        float v;
        memcpy(&v, &bits, sizeof(v));
        g_state.affinity = clampf(v, 0.0f, 1.0f);
        ESP_LOGI(TAG, "affinity carregada: %.3f", g_state.affinity);
    } else {
        ESP_LOGI(TAG, "affinity não encontrada no NVS — usando 0");
    }
    nvs_close(h);
}

/* ── state_vector_save ───────────────────────────────────────────────── */
void state_vector_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open (write) falhou (0x%x)", err);
        return;
    }
    uint32_t bits;
    memcpy(&bits, &g_state.affinity, sizeof(bits));
    nvs_set_u32(h, NVS_KEY_AFFINITY, bits);
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGD(TAG, "affinity salva: %.3f", g_state.affinity);
}

/* ── StateTask: tick a cada 1 s, Core 0, pri 3 ──────────────────────── */
static void s_state_task(void *arg)
{
    (void)arg;
    TickType_t xLastWake = xTaskGetTickCount();
    for (;;) {
        state_vector_tick(now_ms());
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1000));
    }
}

/* ── state_vector_init ───────────────────────────────────────────────── */
void state_vector_init(void)
{
    memset(&g_state, 0, sizeof(g_state));

    /* Defaults iniciais */
    g_state.energy        = 0.80f;
    g_state.mood_valence  = 0.00f;
    g_state.mood_arousal  = 0.20f;
    g_state.social_need   = 0.00f;
    g_state.attention     = 0.50f;
    g_state.comfort       = 0.80f;
    g_state.affinity      = 0.00f;
    g_state.battery_pct   = 100.0f;

    state_vector_load();   /* sobrescreve affinity se existir no NVS */
    blink_set_energy(g_state.energy);

    xTaskCreatePinnedToCore(s_state_task, "StateTask",
                            3072, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "init  energy=%.2f valence=%.2f affinity=%.3f",
             g_state.energy, g_state.mood_valence, g_state.affinity);
}

/* ── state_vector_tick ───────────────────────────────────────────────── */
void state_vector_tick(uint32_t now_ms_val)
{
    static uint32_t s_log_counter  = 0;
    static uint32_t s_save_counter = 0;

    /* Incrementa tempo acordado */
    g_state.time_awake_ms += 1000u;

    /* ── Energy: drain linear 4%/h ──────────────────────────────────── */
    g_state.energy = clampf(g_state.energy - ENERGY_DRAIN_S, 0.0f, 1.0f);

    /* Bonus por música e interação recente (últimos 30 s) */
    if (g_state.music_detected) {
        g_state.energy = clampf(g_state.energy + 0.001f, 0.0f, 1.0f);
    }

    /* E18: blink automático acompanha o nível de energia percebido. */
    blink_set_energy(g_state.energy);

    /* ── Mood valence: retorna a 0 com tau=6h ───────────────────────── */
    g_state.mood_valence = clampf(g_state.mood_valence * DECAY_6H, -1.0f, 1.0f);

    /* ── Mood arousal: retorna a 0 com tau=30min ────────────────────── */
    g_state.mood_arousal = clampf(g_state.mood_arousal * DECAY_30MIN, 0.0f, 1.0f);

    /* ── Social need: sobe sem interação, cai com presença ─────────── */
    {
        const uint32_t since_ms = now_ms_val - g_state.last_interaction_ms;
        if (g_state.person_present) {
            /* Presença ativa: drena social_need (tau ≈ 10 min) */
            g_state.social_need = clampf(
                g_state.social_need * DECAY_10MIN, 0.0f, 1.0f);
        } else if (since_ms > 5000u) {
            /* Isolamento: sobe linearmente */
            g_state.social_need = clampf(
                g_state.social_need + SOCIAL_RISE_S, 0.0f, 1.0f);
        }
    }

    /* ── Attention: decai com tau=10min ─────────────────────────────── */
    if (!g_state.person_present) {
        g_state.attention = clampf(g_state.attention * DECAY_10MIN, 0.0f, 1.0f);
    }

    /* ── Comfort: influência da bateria ─────────────────────────────── */
    {
        float bat_comfort = g_state.battery_pct * 0.01f;   /* 0–1 */
        /* LPF suave: comfort se aproxima do nível de bateria */
        g_state.comfort += (bat_comfort - g_state.comfort) * 0.0001f;
        g_state.comfort  = clampf(g_state.comfort, 0.0f, 1.0f);
    }

    /* ── Auto-save affinity a cada 10 min ───────────────────────────── */
    if (++s_save_counter >= 600u) {
        s_save_counter = 0;
        state_vector_save();
    }

    /* ── Log a cada 60 ticks ─────────────────────────────────────────── */
    if (++s_log_counter >= 60u) {
        s_log_counter = 0;
        ESP_LOGI(TAG,
                 "[STATE] energy=%.2f valence=%.2f arousal=%.2f "
                 "social=%.2f attn=%.2f comfort=%.2f affinity=%.2f",
                 g_state.energy, g_state.mood_valence, g_state.mood_arousal,
                 g_state.social_need, g_state.attention,
                 g_state.comfort, g_state.affinity);
    }
}

/* ── state_vector_on_interaction ─────────────────────────────────────── */
void state_vector_on_interaction(void)
{
    g_state.last_interaction_ms = now_ms();
    g_state.session_interactions++;
    g_state.total_interactions++;

    /* Interação reduz necessidade social, aumenta atenção e melhora valência */
    g_state.social_need  = clampf(g_state.social_need  - 0.30f, 0.0f,  1.0f);
    g_state.attention    = clampf(g_state.attention    + 0.20f, 0.0f,  1.0f);
    g_state.mood_valence = clampf(g_state.mood_valence + 0.04f, -1.0f, 1.0f);
    g_state.mood_arousal = clampf(g_state.mood_arousal + 0.05f, 0.0f,  1.0f);

    ESP_LOGD(TAG, "interaction #%lu  social=%.2f  attn=%.2f",
             (unsigned long)g_state.total_interactions,
             g_state.social_need, g_state.attention);
}

/* ── state_vector_on_touch ───────────────────────────────────────────── */
void state_vector_on_touch(bool rough)
{
    g_state.being_touched = true;
    state_vector_on_interaction();

    if (rough) {
        /* Toque brusco: desconforto, susto */
        g_state.comfort      = clampf(g_state.comfort      - 0.10f, 0.0f, 1.0f);
        g_state.mood_valence = clampf(g_state.mood_valence - 0.08f, -1.0f, 1.0f);
        g_state.mood_arousal = clampf(g_state.mood_arousal + 0.15f,  0.0f, 1.0f);
    } else {
        /* Toque gentil: conforto leve */
        g_state.comfort      = clampf(g_state.comfort      + 0.02f, 0.0f, 1.0f);
        g_state.mood_valence = clampf(g_state.mood_valence + 0.03f, -1.0f, 1.0f);
        g_state.mood_arousal = clampf(g_state.mood_arousal + 0.05f,  0.0f, 1.0f);
    }
}

/* ── state_vector_on_pet ─────────────────────────────────────────────── */
void state_vector_on_pet(void)
{
    state_vector_on_interaction();

    /* Carinho: incremento lento e permanente de affinity */
    g_state.affinity     = clampf(g_state.affinity     + 0.001f, 0.0f, 1.0f);
    g_state.mood_valence = clampf(g_state.mood_valence + 0.06f, -1.0f, 1.0f);
    g_state.social_need  = clampf(g_state.social_need  - 0.15f,  0.0f, 1.0f);
    g_state.comfort      = clampf(g_state.comfort      + 0.03f,  0.0f, 1.0f);

    ESP_LOGD(TAG, "pet  affinity=%.3f  valence=%.2f",
             g_state.affinity, g_state.mood_valence);
}
