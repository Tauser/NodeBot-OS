#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <inttypes.h>

#include "config_manager.h"
#include "nvs_defaults.h"

static const char *TAG = "test_cfg";

/* ── assertiva ───────────────────────────────────────────────────────── */
static uint32_t s_pass, s_fail;

#define ASSERT(cond, msg)                                                    \
    do {                                                                      \
        if (cond) {                                                           \
            ESP_LOGI(TAG, "  PASS  %s", msg);                                \
            s_pass++;                                                         \
        } else {                                                              \
            ESP_LOGE(TAG, "  FAIL  %s  (%s:%d)", msg, __FILE__, __LINE__);   \
            s_fail++;                                                         \
        }                                                                     \
    } while (0)

/* ── helper: lê CRC armazenado diretamente no NVS ───────────────────── */
static uint32_t read_raw_crc(void)
{
    nvs_handle_t h;
    uint32_t crc = 0;
    if (nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, CONFIG_CRC_KEY, &crc);
        nvs_close(h);
    }
    return crc;
}

static void write_raw_crc(uint32_t bad_crc)
{
    nvs_handle_t h;
    if (nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, CONFIG_CRC_KEY, bad_crc);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* ════════════════════════════════════════════════════════════════════════ */

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "  ConfigManager — teste unitário E13");
    ESP_LOGI(TAG, "════════════════════════════════════════");

    /* ── T1: init (estado limpo — factory reset forçado) ─────────────── */
    ESP_LOGI(TAG, "T1: init");

    /* Garante estado limpo: apaga o namespace antes do primeiro init */
    nvs_flash_init();
    nvs_handle_t h_clean;
    if (nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &h_clean) == ESP_OK) {
        nvs_erase_all(h_clean);
        nvs_commit(h_clean);
        nvs_close(h_clean);
    }

    ASSERT(config_manager_init() == ESP_OK, "init retorna ESP_OK");

    /* ── T2: schema version ──────────────────────────────────────────── */
    ESP_LOGI(TAG, "T2: schema version");
    ASSERT(config_get_schema_version() == CONFIG_SCHEMA_VERSION,
           "schema_ver == CONFIG_SCHEMA_VERSION");

    /* ── T3: defaults carregados corretamente ────────────────────────── */
    ESP_LOGI(TAG, "T3: defaults");
    ASSERT(config_get_int("led_bright",   -1) == 128, "led_bright  == 128");
    ASSERT(config_get_int("disp_bright",  -1) == 200, "disp_bright == 200");
    ASSERT(config_get_int("disp_timeout", -1) == 30,  "disp_timeout== 30");
    ASSERT(config_get_int("audio_vol",    -1) == 80,  "audio_vol   == 80");
    ASSERT(config_get_int("wifi_en",      -1) == 0,   "wifi_en     == 0");

    /* ── T4: set e get ───────────────────────────────────────────────── */
    ESP_LOGI(TAG, "T4: set e get");
    ASSERT(config_set_int("led_bright", 42) == ESP_OK, "set led_bright=42");
    ASSERT(config_get_int("led_bright", -1) == 42,     "get led_bright==42");

    ASSERT(config_set_int("audio_vol", 55) == ESP_OK,  "set audio_vol=55");
    ASSERT(config_get_int("audio_vol", -1) == 55,      "get audio_vol==55");

    /* ── T5: write-only-if-changed ───────────────────────────────────── */
    /*
     * Definimos "led_bright" duas vezes com o mesmo valor.
     * Ambas retornam ESP_OK, mas a segunda não deve tocar a flash.
     * Verificamos indiretamente: o CRC não muda na segunda chamada.
     */
    ESP_LOGI(TAG, "T5: write-only-if-changed");
    config_set_int("led_bright", 99);
    uint32_t crc_before = read_raw_crc();
    esp_err_t ret2 = config_set_int("led_bright", 99); /* mesmo valor */
    uint32_t crc_after = read_raw_crc();

    ASSERT(ret2 == ESP_OK,           "set mesmo valor retorna ESP_OK");
    ASSERT(crc_before == crc_after,  "CRC não muda em set sem alteração");

    /* ── T6: CRC armazenado é não-zero e coerente ────────────────────── */
    ESP_LOGI(TAG, "T6: CRC coerência");
    uint32_t stored_crc = read_raw_crc();
    ASSERT(stored_crc != 0, "CRC armazenado != 0");
    ESP_LOGI(TAG, "  CRC atual = 0x%08" PRIX32, stored_crc);

    /* CRC deve mudar ao alterar um valor */
    config_set_int("ble_en", 1);
    uint32_t crc_new = read_raw_crc();
    ASSERT(crc_new != stored_crc, "CRC muda após alteração de valor");

    /* ── T7: CRC inválido → factory reset no próximo init ────────────── */
    ESP_LOGI(TAG, "T7: CRC inválido -> factory reset automatico");

    config_set_int("led_bright", 77);   /* valor diferente do default */
    ASSERT(config_get_int("led_bright", -1) == 77, "led_bright=77 antes da corrupção");

    write_raw_crc(0xDEADBEEF);          /* corrompe o CRC no NVS */

    ASSERT(config_manager_init() == ESP_OK, "re-init após CRC corrompido retorna ESP_OK");
    ASSERT(config_get_int("led_bright", -1) == 128,
           "led_bright restaurado para 128 após CRC mismatch");
    ASSERT(config_get_schema_version() == CONFIG_SCHEMA_VERSION,
           "schema_ver restaurado após reset");

    /* ── T8: factory_reset explícito ────────────────────────────────── */
    ESP_LOGI(TAG, "T8: factory_reset explicito");
    config_set_int("disp_bright", 50);
    ASSERT(config_get_int("disp_bright", -1) == 50,  "disp_bright=50 antes do reset");

    ASSERT(config_factory_reset() == ESP_OK, "factory_reset retorna ESP_OK");
    ASSERT(config_get_int("disp_bright", -1) == 200, "disp_bright restaurado para 200");
    ASSERT(config_get_int("touch_thr",   -1) == 150, "touch_thr restaurado para 150");

    /* CRC deve ser válido após factory reset */
    uint32_t crc_post_reset = read_raw_crc();
    ASSERT(crc_post_reset != 0, "CRC não-zero após factory reset");

    /* ── T9: chave inexistente retorna default ───────────────────────── */
    ESP_LOGI(TAG, "T9: chave inexistente");
    ASSERT(config_get_int("nao_existe", 42) == 42,
           "chave inexistente retorna default_val");
    ASSERT(config_get_int("nao_existe", -7) == -7,
           "chave inexistente retorna default_val negativo");

    /* ── Resultado ───────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "  Resultado: PASS=%" PRIu32 "  FAIL=%" PRIu32, s_pass, s_fail);
    if (s_fail == 0)
        ESP_LOGI(TAG, "  ✓ todos os testes passaram");
    else
        ESP_LOGE(TAG, "  ✗ %" PRIu32 " teste(s) falharam", s_fail);
    ESP_LOGI(TAG, "════════════════════════════════════════");
}
