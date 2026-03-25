#include "config_manager.h"
#include "nvs_defaults.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_check.h"
#include <inttypes.h>
#include <string.h>

static const char *TAG = "cfg_mgr";

_Static_assert(CONFIG_KEY_COUNT <= 64,
               "Aumente o array vals[] em compute_crc");

/* ── CRC32 software (IEEE 802.3, sem tabela) ─────────────────────────── */

static uint32_t crc32_sw(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    }
    return ~crc;
}

/* ── CRC sobre os valores de CONFIG_DEFAULTS (ordem fixa) ────────────── */
/*
 * Lê cada chave do handle NVS (usando o default se ausente) e calcula
 * CRC32 sobre o array de int32_t resultante.
 * Chamado tanto em leitura (READONLY) quanto em escrita (READWRITE) —
 * após nvs_set_i32 o valor já está no cache do handle, antes do commit.
 */
static uint32_t compute_crc(nvs_handle_t h)
{
    int32_t vals[CONFIG_KEY_COUNT];
    int i = 0;

#define X(k, d) {                           \
        int32_t v = (int32_t)(d);           \
        nvs_get_i32(h, k, &v);             \
        vals[i++] = v;                      \
    }
    CONFIG_DEFAULTS(X)
#undef X

    return crc32_sw((const uint8_t *)vals, (size_t)i * sizeof(int32_t));
}

/* ══════════════════════════════════════════════════════════════════════
   API pública
   ══════════════════════════════════════════════════════════════════════ */

esp_err_t config_factory_reset(void)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &h),
                        TAG, "open para factory reset");

    nvs_erase_all(h);

#define X(k, d)  nvs_set_i32(h, k, (int32_t)(d));
    CONFIG_DEFAULTS(X)
#undef X

    uint32_t crc = compute_crc(h);
    nvs_set_u32(h, CONFIG_CRC_KEY, crc);

    esp_err_t ret = nvs_commit(h);
    nvs_close(h);

    if (ret == ESP_OK)
        ESP_LOGI(TAG, "factory reset OK  schema_ver=%d  crc=0x%08" PRIX32,
                 CONFIG_SCHEMA_VERSION, crc);
    return ret;
}

esp_err_t config_manager_init(void)
{
    /* nvs_flash_init é idempotente; trata partição corrompida */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrompido — apagando partição");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_flash_init");

    nvs_handle_t h;
    ret = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "namespace ausente — primeiro boot, carregando defaults");
        return config_factory_reset();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "open");

    /* Lê CRC armazenado */
    uint32_t stored   = 0;
    esp_err_t crc_ret = nvs_get_u32(h, CONFIG_CRC_KEY, &stored);

    /* Computa CRC dos valores atuais */
    uint32_t computed = compute_crc(h);
    nvs_close(h);

    if (crc_ret != ESP_OK || stored != computed) {
        ESP_LOGW(TAG,
                 "CRC inválido  stored=0x%08" PRIX32
                 "  computed=0x%08" PRIX32 " — factory reset",
                 stored, computed);
        return config_factory_reset();
    }

    ESP_LOGI(TAG, "init OK  schema_ver=%" PRId32 "  crc=0x%08" PRIX32,
             config_get_int("schema_ver", 0), computed);
    return ESP_OK;
}

int32_t config_get_int(const char *key, int32_t default_val)
{
    nvs_handle_t h;
    if (nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &h) != ESP_OK)
        return default_val;

    int32_t val = default_val;
    nvs_get_i32(h, key, &val);
    nvs_close(h);
    return val;
}

esp_err_t config_set_int(const char *key, int32_t val)
{
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &h),
                        TAG, "open para set");

    /* Write-only-if-changed: ~val é sempre != val em complemento a dois */
    int32_t current = ~val;
    nvs_get_i32(h, key, &current);

    if (current == val) {
        nvs_close(h);
        return ESP_OK;   /* sem mudança — não acessa a flash */
    }

    esp_err_t ret = nvs_set_i32(h, key, val);
    if (ret == ESP_OK) {
        /* O cache do handle já reflete o novo valor antes do commit */
        uint32_t crc = compute_crc(h);
        ret = nvs_set_u32(h, CONFIG_CRC_KEY, crc);
    }
    if (ret == ESP_OK)
        ret = nvs_commit(h);

    nvs_close(h);
    return ret;
}

uint16_t config_get_schema_version(void)
{
    return (uint16_t)config_get_int("schema_ver", 0);
}
