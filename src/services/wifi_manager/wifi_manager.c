#include "wifi_manager.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <string.h>

static const char *TAG = "wifi";

#define NVS_NS       "wb_wifi"
#define MAX_SSID     32
#define MAX_PASS     64
#define MAX_RETRY     5

static volatile bool s_connected   = false;
static int           s_retry_count = 0;
static TimerHandle_t s_duty_timer  = NULL;
static uint32_t      s_duty_on_ms  = 0;
static uint32_t      s_duty_off_ms = 0;
static bool          s_duty_phase  = true;   /* true = on phase */

/* ── Event handler ─────────────────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            s_connected = false;
            if (s_retry_count < MAX_RETRY) {
                s_retry_count++;
                ESP_LOGI(TAG, "reconectar %d/%d", s_retry_count, MAX_RETRY);
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "max tentativas — WiFi inativo");
            }
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        s_connected   = true;
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "conectado — IP: " IPSTR, IP2STR(&ev->ip_info.ip));
    }
}

/* ── Duty cycle timer ──────────────────────────────────────────────────── */

static void duty_cb(TimerHandle_t t)
{
    if (s_duty_phase) {
        esp_wifi_stop();
        s_duty_phase = false;
        xTimerChangePeriod(t, pdMS_TO_TICKS(s_duty_off_ms), 0);
    } else {
        esp_wifi_start();
        s_duty_phase = true;
        xTimerChangePeriod(t, pdMS_TO_TICKS(s_duty_on_ms), 0);
    }
}

/* ── API pública ───────────────────────────────────────────────────────── */

esp_err_t wifi_manager_init(void)
{
    /* Inicialização de netif/event — idempotente */
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) return ret;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         wifi_event_handler, NULL, NULL);

    /* Lê credenciais do NVS */
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) != ESP_OK) {
        ESP_LOGW(TAG, "sem credenciais no NVS — WiFi inativo");
        return ESP_OK;
    }

    char ssid[MAX_SSID + 1] = {0};
    char pass[MAX_PASS + 1] = {0};
    size_t sl = sizeof(ssid), pl = sizeof(pass);
    bool ok = (nvs_get_str(nvs, "ssid", ssid, &sl) == ESP_OK) &&
              (nvs_get_str(nvs, "pass", pass, &pl) == ESP_OK);
    nvs_close(nvs);

    if (!ok || ssid[0] == '\0') {
        ESP_LOGW(TAG, "credenciais inválidas — WiFi inativo");
        return ESP_OK;
    }

    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid,     ssid, sizeof(wcfg.sta.ssid)     - 1);
    strncpy((char *)wcfg.sta.password, pass, sizeof(wcfg.sta.password) - 1);
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "conectando a '%s'", ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t ret = nvs_open(NVS_NS, NVS_READWRITE, &nvs);
    if (ret != ESP_OK) return ret;
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", password);
    nvs_commit(nvs);
    nvs_close(nvs);

    /* Reconectar com novas credenciais */
    s_retry_count = 0;
    wifi_config_t wcfg = {0};
    strncpy((char *)wcfg.sta.ssid,     ssid,     sizeof(wcfg.sta.ssid)     - 1);
    strncpy((char *)wcfg.sta.password, password, sizeof(wcfg.sta.password) - 1);
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    esp_wifi_disconnect();
    esp_wifi_connect();

    ESP_LOGI(TAG, "credenciais atualizadas — reconectando a '%s'", ssid);
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

void wifi_manager_start_duty_cycle(uint32_t on_ms, uint32_t off_ms)
{
    s_duty_on_ms  = on_ms;
    s_duty_off_ms = off_ms;
    s_duty_phase  = true;

    if (!s_duty_timer) {
        s_duty_timer = xTimerCreate("wifi_dc", pdMS_TO_TICKS(on_ms),
                                     pdFALSE, NULL, duty_cb);
    }
    if (s_duty_timer) xTimerStart(s_duty_timer, 0);
}

void wifi_manager_stop_duty_cycle(void)
{
    if (s_duty_timer) xTimerStop(s_duty_timer, 0);
    esp_wifi_start();
}
