#include "ota_manager.h"

#include "state_vector.h"
#include "motion_safety_service.h"
#include "dialogue_state_service.h"

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "mbedtls/sha256.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "ota";

/* ── Configuração ─────────────────────────────────────────────────────────── */
#define OTA_BUF_SIZE          4096u
#define SIG_MAX_SIZE            72u   /* ECDSA P-256 DER: tipicamente 70-72 bytes */
#define STABLE_TIMEOUT_US  (60LL * 1000000LL)
#define HEARTBEAT_STABLE_MS 60000u   /* 60 s de heartbeats para considerar estável */

/*
 * Chave pública ECDSA P-256 do build system (ponto não-comprimido, 65 bytes).
 *
 * ATENÇÃO: substituir pelo par gerado com:
 *   openssl ecparam -name prime256v1 -genkey -noout -out ota_key.pem
 *   openssl ec -in ota_key.pem -pubout -outform DER | tail -c 65 > ota_pubkey.bin
 *
 * Enquanto esta chave for zeros, toda verificação de assinatura FALHARÁ
 * intencionalmente — protegendo contra OTAs não assinados.
 */
static const uint8_t OTA_PUBLIC_KEY[65] = { 0x00 };  /* PLACEHOLDER — substituir */

/* ── Estado interno ───────────────────────────────────────────────────────── */
static bool             s_pending_verify      = false;
static bool             s_stable              = false;
static esp_timer_handle_t s_watchdog          = NULL;
static uint32_t         s_heartbeat_accum_ms  = 0u;

/* ── Watchdog de rollback ─────────────────────────────────────────────────── */

static void watchdog_cb(void *arg)
{
    (void)arg;
    if (!s_stable) {
        ESP_LOGE(TAG, "WATCHDOG: novo firmware não confirmou estabilidade em 60s — reiniciando para rollback");
        esp_restart();
    }
}

/* ── Verificação de janela de segurança ───────────────────────────────────── */

static esp_err_t check_safety_window(void)
{
    if (g_state.battery_pct <= 30.0f) {
        ESP_LOGW(TAG, "OTA rejeitado: bateria %.1f%% (mínimo 30%%)", (double)g_state.battery_pct);
        return ESP_ERR_INVALID_STATE;
    }
    if (!motion_safety_is_safe()) {
        ESP_LOGW(TAG, "OTA rejeitado: servos em estado de emergência");
        return ESP_ERR_INVALID_STATE;
    }
    if (dialogue_state_get() != DIALOGUE_IDLE) {
        ESP_LOGW(TAG, "OTA rejeitado: conversação ativa (estado=%d)", (int)dialogue_state_get());
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

/* ── Verificação ECDSA P-256 ──────────────────────────────────────────────── */

static esp_err_t verify_ecdsa(const uint8_t *hash32, const uint8_t *sig, size_t sig_len)
{
    /* Chave placeholder (zeros) — rejeita tudo intencionalmente até substituição */
    bool all_zero = true;
    for (int i = 0; i < 65; i++) {
        if (OTA_PUBLIC_KEY[i] != 0x00) { all_zero = false; break; }
    }
    if (all_zero) {
        ESP_LOGE(TAG, "chave pública OTA não configurada (placeholder) — rejeito firmware");
        return ESP_ERR_NOT_SUPPORTED;
    }

    mbedtls_ecdsa_context ctx;
    mbedtls_ecdsa_init(&ctx);

    int rc = mbedtls_ecp_group_load(&ctx.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (rc != 0) { mbedtls_ecdsa_free(&ctx); return ESP_FAIL; }

    rc = mbedtls_ecp_point_read_binary(
            &ctx.MBEDTLS_PRIVATE(grp), &ctx.MBEDTLS_PRIVATE(Q),
            OTA_PUBLIC_KEY, sizeof(OTA_PUBLIC_KEY));
    if (rc != 0) {
        ESP_LOGE(TAG, "chave pública inválida (rc=-%04X)", (unsigned)(-rc));
        mbedtls_ecdsa_free(&ctx);
        return ESP_ERR_INVALID_ARG;
    }

    rc = mbedtls_ecdsa_read_signature(&ctx, hash32, 32, sig, sig_len);
    mbedtls_ecdsa_free(&ctx);

    if (rc != 0) {
        ESP_LOGE(TAG, "assinatura ECDSA inválida (rc=-%04X)", (unsigned)(-rc));
        return ESP_ERR_INVALID_CRC;
    }
    ESP_LOGI(TAG, "assinatura ECDSA OK");
    return ESP_OK;
}

/* ── Download da assinatura ───────────────────────────────────────────────── */

static esp_err_t download_signature(const char *sig_url, uint8_t *buf, size_t *out_len)
{
    esp_http_client_config_t cfg = {
        .url              = sig_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms       = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) { esp_http_client_cleanup(client); return err; }

    esp_http_client_fetch_headers(client);

    int read = esp_http_client_read(client, (char *)buf, SIG_MAX_SIZE);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read <= 0) return ESP_ERR_NOT_FOUND;
    *out_len = (size_t)read;
    return ESP_OK;
}

/* ── Download principal + cálculo de hash ────────────────────────────────── */

static esp_err_t download_and_flash(const char *url,
                                     const esp_partition_t *part,
                                     uint8_t *hash32_out)
{
    uint8_t *buf = malloc(OTA_BUF_SIZE);
    if (!buf) return ESP_ERR_NO_MEM;

    esp_http_client_config_t cfg = {
        .url               = url,
        .crt_bundle_attach  = esp_crt_bundle_attach,
        .timeout_ms        = 30000,
        .buffer_size       = OTA_BUF_SIZE,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) { free(buf); return ESP_ERR_NO_MEM; }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao abrir conexão: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client); free(buf); return err;
    }

    int content_len = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "firmware size=%d bytes", content_len);

    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        esp_http_client_close(client); esp_http_client_cleanup(client); free(buf); return err;
    }

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);  /* 0 = SHA-256 */

    size_t total = 0u;
    int got;
    while ((got = esp_http_client_read(client, (char *)buf, OTA_BUF_SIZE)) > 0) {
        err = esp_ota_write(ota_handle, buf, (size_t)got);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            mbedtls_sha256_free(&sha_ctx);
            esp_http_client_close(client); esp_http_client_cleanup(client); free(buf);
            return err;
        }
        mbedtls_sha256_update(&sha_ctx, buf, (size_t)got);
        total += (size_t)got;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buf);

    mbedtls_sha256_finish(&sha_ctx, hash32_out);
    mbedtls_sha256_free(&sha_ctx);

    if (total == 0u) {
        esp_ota_abort(ota_handle);
        ESP_LOGE(TAG, "download vazio");
        return ESP_ERR_NOT_FOUND;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "download ok — %u bytes, SHA256 calculado", (unsigned)total);
    return ESP_OK;
}

/* ── API pública ──────────────────────────────────────────────────────────── */

esp_err_t ota_manager_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {

        s_pending_verify    = true;
        s_stable            = false;
        s_heartbeat_accum_ms = 0u;

        esp_timer_create_args_t ta = {
            .callback = watchdog_cb,
            .name     = "ota_wdg",
        };
        esp_err_t err = esp_timer_create(&ta, &s_watchdog);
        if (err == ESP_OK) {
            esp_timer_start_once(s_watchdog, STABLE_TIMEOUT_US);
            ESP_LOGW(TAG, "firmware pendente de verificação — watchdog 60s iniciado");
        } else {
            ESP_LOGE(TAG, "falha ao criar watchdog: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(TAG, "firmware verificado — sem rollback pendente");
    }

    return ESP_OK;
}

esp_err_t ota_manager_check_and_apply(const char *url)
{
    if (!url) return ESP_ERR_INVALID_ARG;

    /* 1. Janela de segurança */
    esp_err_t err = check_safety_window();
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "janela OK — iniciando OTA de %s", url);

    /* 2. Partição inativa */
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        ESP_LOGE(TAG, "partição de atualização não encontrada");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "escrevendo em partição: %s (offset=0x%08"PRIx32")",
             update_part->label, update_part->address);

    /* 3. Download + hash */
    uint8_t hash[32];
    err = download_and_flash(url, update_part, hash);
    if (err != ESP_OK) return err;

    /* 4. Download da assinatura */
    size_t sig_url_len = strlen(url) + 5u;  /* ".sig\0" */
    char *sig_url = malloc(sig_url_len);
    if (!sig_url) return ESP_ERR_NO_MEM;
    snprintf(sig_url, sig_url_len, "%s.sig", url);

    uint8_t sig[SIG_MAX_SIZE];
    size_t  sig_len = 0u;
    err = download_signature(sig_url, sig, &sig_len);
    free(sig_url);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao baixar assinatura: %s", esp_err_to_name(err));
        return err;
    }

    /* 5. Verificação ECDSA ANTES do swap */
    err = verify_ecdsa(hash, sig, sig_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "assinatura inválida — abortando OTA");
        return err;
    }

    /* 6. Swap de partição + reinício */
    err = esp_ota_set_boot_partition(update_part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA OK — reiniciando para novo firmware");
    vTaskDelay(pdMS_TO_TICKS(500));  /* flush de logs */
    esp_restart();
    return ESP_OK;  /* nunca alcançado */
}

void ota_manager_mark_stable(void)
{
    if (s_stable) return;
    s_stable = true;

    if (s_watchdog) {
        esp_timer_stop(s_watchdog);
        esp_timer_delete(s_watchdog);
        s_watchdog = NULL;
    }

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "firmware marcado como estável — rollback cancelado");
    } else {
        ESP_LOGW(TAG, "esp_ota_mark_app_valid_cancel_rollback: %s", esp_err_to_name(err));
    }
}

void ota_manager_send_heartbeat(void)
{
    if (!s_pending_verify || s_stable) return;

    /* Acumula tempo (chamado a cada tick do BehaviorLoop = 100ms) */
    s_heartbeat_accum_ms += 100u;

    if (s_heartbeat_accum_ms >= HEARTBEAT_STABLE_MS) {
        ESP_LOGI(TAG, "60s de operação confirmados — marcando firmware estável");
        ota_manager_mark_stable();
    }
}

bool ota_manager_is_pending_verify(void)
{
    return s_pending_verify && !s_stable;
}
