#include "diagnostic_mode.h"
#include "state_vector.h"
#include "event_bus.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>

static const char *TAG = "diag";

#define TRIPLE_WINDOW_MS  2000u
#define ACTIVE_TIMEOUT_MS 60000u
#define PRINT_INTERVAL_MS  2000u

static volatile bool s_active       = false;
static uint32_t      s_tap_count    = 0;
static uint32_t      s_first_tap_ms = 0;
static uint32_t      s_active_since = 0;
static TaskHandle_t  s_task         = NULL;

static uint32_t read_crash_count(void)
{
    nvs_handle_t nvs;
    uint32_t v = 0;
    if (nvs_open("nb_safety", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "crash_count", &v);
        nvs_close(nvs);
    }
    return v;
}

static void print_diag(void)
{
    ESP_LOGI(TAG,
        "{\"heap\":%"PRIu32",\"psram\":%"PRIu32","
        "\"energy\":%.2f,\"valence\":%.2f,\"arousal\":%.2f,"
        "\"battery\":%.1f,\"crash_count\":%"PRIu32"}",
        (uint32_t)esp_get_free_heap_size(),
        (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (double)g_state.energy,
        (double)g_state.mood_valence,
        (double)g_state.mood_arousal,
        (double)g_state.battery_pct,
        read_crash_count());
}

static void diag_task(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(PRINT_INTERVAL_MS));
        if (!s_active) continue;

        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000LL);
        if (now - s_active_since >= ACTIVE_TIMEOUT_MS) {
            s_active = false;
            ESP_LOGI(TAG, "modo diagnóstico desativado (timeout)");
            continue;
        }
        print_diag();
    }
}

static void on_touch(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000LL);

    if (s_tap_count == 0 || (now - s_first_tap_ms) > TRIPLE_WINDOW_MS) {
        s_tap_count    = 1;
        s_first_tap_ms = now;
        return;
    }

    s_tap_count++;
    if (s_tap_count >= 3) {
        s_tap_count = 0;
        if (!s_active) {
            s_active       = true;
            s_active_since = now;
            ESP_LOGI(TAG, "modo diagnóstico ATIVO — 60s");
            print_diag();
        } else {
            s_active = false;
            ESP_LOGI(TAG, "modo diagnóstico desativado");
        }
    }
}

esp_err_t diagnostic_mode_init(void)
{
    event_bus_subscribe(EVT_TOUCH_PRESS, on_touch);

    BaseType_t ret = xTaskCreatePinnedToCore(diag_task, "diag_mode",
                                             2048, NULL, 5, &s_task, 1);
    if (ret != pdPASS) return ESP_FAIL;
    ESP_LOGI(TAG, "ok — ativar com 3 toques em < 2s");
    return ESP_OK;
}

bool diagnostic_mode_is_active(void)
{
    return s_active;
}
