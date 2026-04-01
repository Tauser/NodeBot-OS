#include "boot_sequence.h"

/* Plataforma */
#include "hal_init.h"

/* Managers */
#include "config_manager.h"
#include "log_manager.h"
#include "event_bus.h"

/* Drivers */
#include "sd_driver.h"
#include "display.h"
#include "face_engine.h"
#include "face_debug.h"
#include "blink_controller.h"
#include "imu_service.h"
#include "touch_service.h"
#include "audio_capture.h"
#include "audio_feedback.h"
#include "wake_word.h"
#include "ws2812_driver.h"
#include "audio_driver.h"
#include "gaze_service.h"
#include "state_vector.h"
#include "idle_behavior.h"
#include "brownout_handler.h"
#include "safe_mode_service.h"
#include "led_router.h"
#include "intent_mapper.h"
#include "tts.h"
#include "dialogue_state_service.h"
#include "motion_safety_service.h"
#include "behavior_engine.h"
#include "persona_service.h"
#include "preference_memory_service.h"
#include "mood_service.h"
#include "attention_service.h"
#include "engagement_service.h"
#include "camera_service.h"
#include "camera_bringup.h"
#include "i2c_bus.h"
#include "wifi_manager.h"
#include "cloud_bridge.h"
#include "snapshot_service.h"
#include "diagnostic_mode.h"
#include "telemetry_service.h"
#include "ota_manager.h"
#include "factory_reset.h"
#include "jig_service.h"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdbool.h>

static const char *TAG = "boot";

/* ── estado interno ─────────────────────────────────────────────────── */
static bool      s_log_ready = false;
static esp_err_t s_first_err = ESP_OK;

/* ── stubs para subsistemas não implementados ────────────────────────── */
/* Substituídos automaticamente quando os módulos reais existirem */
__attribute__((weak)) esp_err_t storage_manager_init(void)
{
    return sd_init();
}

__attribute__((weak)) esp_err_t power_manager_init(void)
{
    ESP_LOGI(TAG, "power_manager: stub (não implementado)");
    return ESP_OK;
}

/* ── logging de boot ─────────────────────────────────────────────────── */
static void blog(log_level_t lvl, int step, const char *label, esp_err_t err)
{
    if (!s_log_ready) return;
    char msg[64];
    if (err == ESP_OK)
        snprintf(msg, sizeof(msg), "[STEP %d] %s init_ok", step, label);
    else
        snprintf(msg, sizeof(msg), "[STEP %d] %s init_fail err=0x%08" PRIX32,
                 step, label, (uint32_t)err);
    log_write(lvl, "boot", msg);
}

/* ── macros de passo ─────────────────────────────────────────────────── */

/* Para funções que retornam esp_err_t */
#define BOOT_STEP(n, label, call)                                          \
    do {                                                                    \
        esp_err_t _e = (call);                                             \
        if (_e == ESP_OK) {                                                 \
            ESP_LOGI(TAG, "[STEP %d] %s init_ok", n, label);              \
        } else {                                                            \
            ESP_LOGE(TAG, "[STEP %d] %s init_fail err=0x%08X",            \
                     n, label, (unsigned)_e);                              \
            if (s_first_err == ESP_OK) s_first_err = _e;                  \
        }                                                                   \
        blog((_e == ESP_OK) ? LOG_INFO : LOG_FATAL, n, label, _e);        \
    } while (0)

/* Para funções que retornam void (drivers de baixo nível) */
#define BOOT_STEP_V(n, label, call)                                        \
    do {                                                                    \
        (call);                                                             \
        ESP_LOGI(TAG, "[STEP %d] %s init_ok", n, label);                  \
        blog(LOG_INFO, n, label, ESP_OK);                                  \
    } while (0)

/* ══════════════════════════════════════════════════════════════════════
   app_boot — sequência principal
   ══════════════════════════════════════════════════════════════════════ */

esp_err_t app_boot(void)
{
    s_log_ready = false;
    s_first_err = ESP_OK;

    ESP_LOGI(TAG, "══════════════════════════════════════");
    ESP_LOGI(TAG, "  NodeBot — boot sequence");
    ESP_LOGI(TAG, "══════════════════════════════════════");

    /* ── PRÉ-BOOT: brownout handler (sem NVS ainda, só registo) ─────── */
    brownout_handler_init();

    /* ── STEP 1: HAL ─────────────────────────────────────────────────── */
    /*
     * hal_init.h define apenas constantes de pinos — não há função de init.
     * O "passo" valida que a configuração foi compilada corretamente.
     */
    ESP_LOGI(TAG, "[STEP 1] hal init_ok  sda=%d scl=%d spi_mosi=%d",
             HAL_I2C_SDA, HAL_I2C_SCL, HAL_SPI_MOSI);
    blog(LOG_INFO, 1, "hal", ESP_OK);

    /* ── STEP 2: ConfigManager ───────────────────────────────────────── */
    BOOT_STEP(2, "config_manager", config_manager_init());

    /* Safe mode check logo após NVS disponível */
    BOOT_STEP(2, "safe_mode_check", safe_mode_check());

    /* ── STEP 3: StorageManager (sd_init via weak stub) ─────────────── */
    BOOT_STEP(3, "storage_manager", storage_manager_init());

    /* ── STEP 4: LogManager ──────────────────────────────────────────── */
    {
        esp_err_t _e = log_init();
        if (_e == ESP_OK) {
            s_log_ready = true;   /* habilita log_write() nos passos seguintes */
            ESP_LOGI(TAG, "[STEP 4] log_manager init_ok");
            log_write(LOG_INFO, "boot", "[STEP 4] log_manager init_ok");
        } else {
            ESP_LOGE(TAG, "[STEP 4] log_manager init_fail err=0x%08X", (unsigned)_e);
            if (s_first_err == ESP_OK) s_first_err = _e;
        }
    }

    /* ── STEP 5: Drivers ─────────────────────────────────────────────── */
    BOOT_STEP_V(5, "display",       display_init());
    
    /* Rotação do display: chame a API correspondente do seu driver aqui */
    // BOOT_STEP_V(5, "display_rot", display_set_rotation(1)); /* Ex: 1 = 90 graus */
    
    BOOT_STEP_V(5, "face_engine",   face_engine_init());
    /* DEBUG: calibração via serial — substitui face_engine_start_task() */
    //BOOT_STEP_V(5, "face_debug",    face_debug_start_task(NULL));
    // PRODUÇÃO: trocar a linha acima por:
    BOOT_STEP_V(5, "face_render", face_engine_start_task());
    BOOT_STEP_V(5, "blink_ctrl", blink_controller_init());
    /* ── i2c_bus: barramento compartilhado — ANTES de qualquer driver I2C ── */
    BOOT_STEP(5, "i2c_bus", i2c_bus_init());

    /* E08A bring-up câmera: descomentar para validação HW; manter comentado em produção.
     * DEVE ficar antes de imu_service_init (Regra R1: SCCB não pode disputar I2C_NUM_0). */
    // BOOT_STEP(5, "cam_bringup_init",    camera_bringup_init());
    // BOOT_STEP(5, "cam_bringup_20x",     camera_bringup_capture_n(20, 50));

    /* touch_service_init() e imu_service_init() inicializam seus drivers internamente */
    BOOT_STEP_V(5, "ws2812",     ws2812_init(HAL_RMT_LED, HAL_RMT_LED_COUNT));
    ws2812_set_state(safe_mode_is_active() ? LED_STATE_DEGRADED : LED_STATE_NORMAL);
    BOOT_STEP_V(5, "audio",      audio_init());

    /* ── STEP 6: EventBus ────────────────────────────────────────────── */
    BOOT_STEP(6, "event_bus", event_bus_init());

    BOOT_STEP_V(6, "face_events",   face_engine_register_events());
    BOOT_STEP_V(6, "gaze_service",  gaze_service_init());
    BOOT_STEP_V(6, "state_vector",  state_vector_init());
    BOOT_STEP_V(6, "idle_behavior", idle_behavior_init());
    BOOT_STEP  (6, "led_router",    led_router_init());
    BOOT_STEP  (6, "touch_service", touch_service_init());
    BOOT_STEP  (6, "imu_service",   imu_service_init());
    BOOT_STEP  (6, "audio_capture",  audio_capture_init());
    BOOT_STEP  (6, "audio_feedback", audio_feedback_init());
    BOOT_STEP  (6, "wake_word",      wake_word_init());
    BOOT_STEP  (6, "intent_mapper",  intent_mapper_init());
    BOOT_STEP  (6, "tts",            tts_init());
    BOOT_STEP  (6, "dialogue_state", dialogue_state_service_init());
    BOOT_STEP_V(6, "motion_safety",  motion_safety_init());
    BOOT_STEP  (6, "persona",          persona_service_init());
    BOOT_STEP  (6, "pref_memory",      preference_memory_init());
    BOOT_STEP  (6, "mood_service",     mood_service_init());
    BOOT_STEP  (6, "attention_svc",    attention_service_init());
    BOOT_STEP  (6, "engagement_svc",   engagement_service_init());
    BOOT_STEP  (6, "camera_service",   camera_service_init());
    BOOT_STEP  (6, "behavior_engine",  behavior_engine_init());
    //wifi_manager_set_credentials("CRIARE_2G", "17122484");
    BOOT_STEP  (6, "wifi_manager",     wifi_manager_init());
    BOOT_STEP  (6, "cloud_bridge",     cloud_bridge_init());
    BOOT_STEP  (6, "snapshot_svc",     snapshot_service_init());
    BOOT_STEP  (6, "diagnostic_mode",  diagnostic_mode_init());
    BOOT_STEP  (6, "telemetry_svc",    telemetry_service_init());
    BOOT_STEP  (6, "ota_manager",      ota_manager_init());
    BOOT_STEP  (6, "factory_reset",    factory_reset_init());
    BOOT_STEP  (6, "jig_service",      jig_service_init());
    //wifi_manager_set_credentials("CRIARE_2G", "17122484");  /* provisionamento — use uma vez */
    //cloud_bridge_set_api_key("...");                         /* provisionamento — use uma vez */


    /* ── STEP 7: PowerManager ────────────────────────────────────────── */
    BOOT_STEP(7, "power_manager", power_manager_init());

    /* ── Resultado ───────────────────────────────────────────────────── */
    if (s_first_err == ESP_OK) {
        ESP_LOGI(TAG, "boot OK — todos os subsistemas inicializados");
        if (s_log_ready) log_write(LOG_INFO, "boot", "boot OK");
        /* Inicia contagem de 60s para considerar boot estável */
        if (!safe_mode_is_active()) {
            safe_mode_start_stable_timer();
        }
    } else {
        ESP_LOGW(TAG, "boot concluído com erros (first=0x%08X)", (unsigned)s_first_err);
        if (s_log_ready) log_write(LOG_WARN, "boot", "boot concluído com erros");
    }
    ESP_LOGI(TAG, "══════════════════════════════════════");

    return s_first_err;
}
