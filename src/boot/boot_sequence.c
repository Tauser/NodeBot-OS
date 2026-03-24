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
#include "imu_driver.h"
#include "touch_driver.h"
#include "ws2812_driver.h"
#include "inmp441_driver.h"
#include "max98357a_driver.h"

#include "esp_log.h"
#include "esp_err.h"
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
    BOOT_STEP_V(5, "display",    display_init());
    BOOT_STEP  (5, "imu",        imu_init());
    BOOT_STEP_V(5, "touch",      touch_driver_init());
    BOOT_STEP_V(5, "ws2812",     ws2812_init(HAL_RMT_LED, HAL_RMT_LED_COUNT));
    BOOT_STEP_V(5, "inmp441",    inmp441_init());
    BOOT_STEP_V(5, "max98357a",  max98357a_init());

    /* ── STEP 6: EventBus ────────────────────────────────────────────── */
    BOOT_STEP(6, "event_bus", event_bus_init());

    /* ── STEP 7: PowerManager ────────────────────────────────────────── */
    BOOT_STEP(7, "power_manager", power_manager_init());

    /* ── Resultado ───────────────────────────────────────────────────── */
    if (s_first_err == ESP_OK) {
        ESP_LOGI(TAG, "boot OK — todos os subsistemas inicializados");
        if (s_log_ready) log_write(LOG_INFO, "boot", "boot OK");
    } else {
        ESP_LOGW(TAG, "boot concluído com erros (first=0x%08X)", (unsigned)s_first_err);
        if (s_log_ready) log_write(LOG_WARN, "boot", "boot concluído com erros");
    }
    ESP_LOGI(TAG, "══════════════════════════════════════");

    return s_first_err;
}
