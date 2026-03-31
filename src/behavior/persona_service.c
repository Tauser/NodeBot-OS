#include "persona_service.h"
#include "sd_driver.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "persona";

#define NAME_MAX 32
static char           s_name[NAME_MAX] = "Noisy";
static persona_style_t s_style         = PERSONA_PLAYFUL;

esp_err_t persona_service_init(void)
{
    const char *path = "/sdcard/persona.json";
    if (!sd_file_exists(path)) {
        ESP_LOGI(TAG, "persona.json ausente — usando defaults (name=%s)", s_name);
        return ESP_OK;
    }

    char buf[256];
    size_t n = sd_read_file(path, buf, sizeof(buf) - 1u);
    if (n == 0u) return ESP_OK;
    buf[n] = '\0';

    cJSON *root = cJSON_ParseWithLength(buf, n);
    if (!root) { ESP_LOGW(TAG, "JSON inválido"); return ESP_OK; }

    cJSON *jname = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (cJSON_IsString(jname) && jname->valuestring) {
        strncpy(s_name, jname->valuestring, NAME_MAX - 1u);
        s_name[NAME_MAX - 1u] = '\0';
    }

    cJSON *jstyle = cJSON_GetObjectItemCaseSensitive(root, "response_style");
    if (cJSON_IsString(jstyle) && jstyle->valuestring) {
        if      (strcmp(jstyle->valuestring, "CALM")    == 0) s_style = PERSONA_CALM;
        else if (strcmp(jstyle->valuestring, "MINIMAL") == 0) s_style = PERSONA_MINIMAL;
        else                                                   s_style = PERSONA_PLAYFUL;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "ok — name=%s style=%d", s_name, (int)s_style);
    return ESP_OK;
}

const char *persona_get_name(void)  { return s_name; }
persona_style_t persona_get_style(void) { return s_style; }
