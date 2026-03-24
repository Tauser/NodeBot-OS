#include "sd_driver.h"
#include "hal_init.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

#define MOUNT_POINT "/sdcard"
#define MAX_FILES   4

static const char    *TAG  = "sd";
static sdmmc_card_t  *s_card = NULL;

/* ─────────────────────────────────────────────────────────────────── */

esp_err_t sd_init(void)
{
    if (s_card) return ESP_OK; /* já montado */

    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files              = MAX_FILES,
        .allocation_unit_size   = 16 * 1024,
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk   = HAL_SD_CLK;
    slot.cmd   = HAL_SD_CMD;
    slot.d0    = HAL_SD_DATA0;
    slot.d1    = SDMMC_SLOT_NO_CD;
    slot.d2    = SDMMC_SLOT_NO_CD;
    slot.d3    = SDMMC_SLOT_NO_CD;
    slot.width = 1; /* 1-bit bus */

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot,
                                             &mount, &s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mount falhou: %s", esp_err_to_name(ret));
        return ret;
    }

    sdmmc_card_print_info(stdout, s_card);
    ESP_LOGI(TAG, "montado em %s", MOUNT_POINT);
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────── */

esp_err_t sd_write_file(const char *path, const void *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "write: não abriu %s", path);
        return ESP_FAIL;
    }
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    if (written != len) {
        ESP_LOGE(TAG, "write: %u/%u bytes", (unsigned)written, (unsigned)len);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* ─────────────────────────────────────────────────────────────────── */

size_t sd_read_file(const char *path, void *buf, size_t maxlen)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "read: não abriu %s", path);
        return 0;
    }
    size_t n = fread(buf, 1, maxlen, f);
    fclose(f);
    return n;
}

/* ─────────────────────────────────────────────────────────────────── */

bool sd_file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}
