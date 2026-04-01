#include "factory_reset.h"
#include "event_bus.h"
#include "touch_service.h"
#include "touch_driver.h"
#include "ws2812_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "factory_reset";

/* ── Configuração ─────────────────────────────────────────────────────────── */
#define TAP_ZONE               TOUCH_ZONE_TOP
#define TRIPLE_TAP_WINDOW_MS   3000u    /* 3 toques dentro deste janela dispara */
#define CONFIRM_TIMEOUT_MS     5000u    /* 5s de countdown para confirmar        */
#define BLINK_PERIOD_MS         300u    /* período do piscar âmbar               */

/* Diretórios SD apagados no factory reset */
static const char *k_sd_dirs[] = {
    "/sdcard/snapshots",
    "/sdcard/logs",
    "/sdcard/tts",
    "/sdcard/kws",
    "/sdcard/sounds",
    NULL
};

/* ── Estado ───────────────────────────────────────────────────────────────── */
static uint32_t          s_tap_count      = 0u;
static uint32_t          s_first_tap_ms   = 0u;
static volatile bool     s_confirming     = false;
static esp_timer_handle_t s_confirm_timer = NULL;
static esp_timer_handle_t s_blink_timer   = NULL;
static bool              s_blink_on       = false;

/* ── Limpeza do SD ────────────────────────────────────────────────────────── */

static void remove_dir_recursive(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *entry;
    char full[320];
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_dir_recursive(full);
                rmdir(full);
            } else {
                remove(full);
            }
        }
    }
    closedir(dir);
}

/* ── Execução do factory reset ───────────────────────────────────────────── */

void factory_reset_execute(void)
{
    ESP_LOGW(TAG, "FACTORY RESET — apagando NVS e SD");

    /* LED vermelho fixo durante apagamento */
    ws2812_set_pixel(0, 200, 0, 0);
    ws2812_set_pixel(1, 200, 0, 0);
    ws2812_set_pixel(2, 200, 0, 0);

    /* Apaga NVS */
    nvs_flash_deinit();
    esp_err_t err = nvs_flash_erase();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS apagado");
    } else {
        ESP_LOGE(TAG, "falha ao apagar NVS: %s", esp_err_to_name(err));
    }

    /* Apaga diretórios do SD */
    for (int i = 0; k_sd_dirs[i] != NULL; i++) {
        remove_dir_recursive(k_sd_dirs[i]);
        ESP_LOGI(TAG, "SD limpo: %s", k_sd_dirs[i]);
    }

    /* LED verde antes do reinício */
    ws2812_set_state(LED_STATE_NORMAL);

    ESP_LOGI(TAG, "factory reset concluído — reiniciando");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

/* ── Timers ───────────────────────────────────────────────────────────────── */

static void blink_cb(void *arg)
{
    (void)arg;
    s_blink_on = !s_blink_on;
    if (s_blink_on) {
        /* âmbar = R+G */
        ws2812_set_pixel(0, 180, 80, 0);
        ws2812_set_pixel(1, 180, 80, 0);
        ws2812_set_pixel(2, 180, 80, 0);
    } else {
        ws2812_set_pixel(0, 0, 0, 0);
        ws2812_set_pixel(1, 0, 0, 0);
        ws2812_set_pixel(2, 0, 0, 0);
    }
}

static void confirm_timeout_cb(void *arg)
{
    (void)arg;
    if (!s_confirming) return;

    esp_timer_stop(s_blink_timer);
    s_confirming = false;
    factory_reset_execute();
    /* não retorna */
}

static void start_confirmation(void)
{
    s_confirming = true;
    ESP_LOGW(TAG, "confirmação iniciada — toque novamente para cancelar (5s)");

    esp_timer_start_periodic(s_blink_timer, BLINK_PERIOD_MS * 1000LL);
    esp_timer_start_once(s_confirm_timer, CONFIRM_TIMEOUT_MS * 1000LL);
}

static void cancel_confirmation(void)
{
    s_confirming = false;
    esp_timer_stop(s_confirm_timer);
    esp_timer_stop(s_blink_timer);
    ws2812_set_state(LED_STATE_NORMAL);
    ESP_LOGI(TAG, "factory reset cancelado");
}

/* ── Callback de toque ────────────────────────────────────────────────────── */

static void on_touch(uint16_t type, void *payload)
{
    (void)type;
    if (!payload) return;

    touch_event_t *ev = (touch_event_t *)payload;
    if (ev->zone_id != TAP_ZONE) return;

    /* Se já está confirmando: qualquer toque na zona cancela */
    if (s_confirming) {
        cancel_confirmation();
        return;
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000LL);

    if (s_tap_count == 0u || (now_ms - s_first_tap_ms) > TRIPLE_TAP_WINDOW_MS) {
        s_tap_count    = 1u;
        s_first_tap_ms = now_ms;
        return;
    }

    s_tap_count++;
    if (s_tap_count >= 3u) {
        s_tap_count = 0u;
        start_confirmation();
    }
}

/* ── Init ─────────────────────────────────────────────────────────────────── */

esp_err_t factory_reset_init(void)
{
    esp_timer_create_args_t confirm_args = {
        .callback = confirm_timeout_cb,
        .name     = "fr_confirm",
    };
    esp_err_t err = esp_timer_create(&confirm_args, &s_confirm_timer);
    if (err != ESP_OK) return err;

    esp_timer_create_args_t blink_args = {
        .callback = blink_cb,
        .name     = "fr_blink",
    };
    err = esp_timer_create(&blink_args, &s_blink_timer);
    if (err != ESP_OK) {
        esp_timer_delete(s_confirm_timer);
        return err;
    }

    event_bus_subscribe(EVT_TOUCH_PRESS, on_touch);

    ESP_LOGI(TAG, "ok — 3 toques em ZONE_TOP dentro de 3s para ativar");
    return ESP_OK;
}

bool factory_reset_is_confirming(void)
{
    return s_confirming;
}
