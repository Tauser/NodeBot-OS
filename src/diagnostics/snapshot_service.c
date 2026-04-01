#include "snapshot_service.h"
#include "state_vector.h"
#include "sd_driver.h"
#include "esp_log.h"
#include <sys/stat.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "snapshot";

#define INTERVAL_MS  60000u
#define DIR          "/sdcard/snapshots"
#define BUF_SIZE     384u

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

static void write_snapshot(void)
{
    uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000000LL);
    char path[64];
    snprintf(path, sizeof(path), DIR "/snap_%08"PRIu32".json", ts);

    char buf[BUF_SIZE];
    int n = snprintf(buf, sizeof(buf),
        "{"
        "\"ts\":%"PRIu32","
        "\"energy\":%.3f,"
        "\"valence\":%.3f,"
        "\"arousal\":%.3f,"
        "\"social\":%.3f,"
        "\"attn\":%.3f,"
        "\"comfort\":%.3f,"
        "\"affinity\":%.3f,"
        "\"battery\":%.1f,"
        "\"heap_free\":%"PRIu32","
        "\"psram_free\":%"PRIu32","
        "\"crash_count\":%"PRIu32
        "}\n",
        ts,
        (double)g_state.energy,
        (double)g_state.mood_valence,
        (double)g_state.mood_arousal,
        (double)g_state.social_need,
        (double)g_state.attention,
        (double)g_state.comfort,
        (double)g_state.affinity,
        (double)g_state.battery_pct,
        (uint32_t)esp_get_free_heap_size(),
        (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        read_crash_count());

    if (n <= 0) return;

    /* Garante diretório */
    mkdir(DIR, 0755);

    if (sd_write_file(path, buf, (size_t)n) == ESP_OK) {
        ESP_LOGD(TAG, "snap: %s", path);
    } else {
        ESP_LOGW(TAG, "falha ao escrever %s", path);
    }
}

static void snapshot_task(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_MS));
        write_snapshot();
    }
}

esp_err_t snapshot_service_init(void)
{
    mkdir(DIR, 0755);
    BaseType_t ret = xTaskCreatePinnedToCore(snapshot_task, "snapshot",
                                             2048, NULL, 5, NULL, 1);
    if (ret != pdPASS) return ESP_FAIL;
    ESP_LOGI(TAG, "ok — snapshot a cada 60s em %s", DIR);
    return ESP_OK;
}

void snapshot_service_flush(void)
{
    write_snapshot();
}
