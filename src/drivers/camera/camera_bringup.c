#include "camera_bringup.h"
#include "hal_init.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "cam_bringup"

/* ── Pinos OV2640 — fixos de placa (Freenove ESP32-S3-WROOM CAM) ──── */
#define CAM_PIN_XCLK     15
#define CAM_PIN_PCLK     13
#define CAM_PIN_VSYNC     6
#define CAM_PIN_HREF      7
#define CAM_PIN_SIOD      HAL_I2C_SDA   /* GPIO4  — SCCB/I2C compartilhado */
#define CAM_PIN_SIOC      HAL_I2C_SCL   /* GPIO5  — SCCB/I2C compartilhado */
#define CAM_PIN_D0       11
#define CAM_PIN_D1        9
#define CAM_PIN_D2        8
#define CAM_PIN_D3       10
#define CAM_PIN_D4       12
#define CAM_PIN_D5       18
#define CAM_PIN_D6       17
#define CAM_PIN_D7       16
#define CAM_PIN_PWDN     (-1)
#define CAM_PIN_RESET    (-1)

static bool s_ready = false;

/* ── Init ─────────────────────────────────────────────────────────── */

esp_err_t camera_bringup_init(void)
{
    if (s_ready) {
        ESP_LOGW(TAG, "já inicializado");
        return ESP_OK;
    }

    camera_config_t cfg = {
        .pin_pwdn         = CAM_PIN_PWDN,
        .pin_reset        = CAM_PIN_RESET,
        .pin_xclk         = CAM_PIN_XCLK,
        .pin_sccb_sda     = CAM_PIN_SIOD,
        .pin_sccb_scl     = CAM_PIN_SIOC,
        .pin_d7           = CAM_PIN_D7,
        .pin_d6           = CAM_PIN_D6,
        .pin_d5           = CAM_PIN_D5,
        .pin_d4           = CAM_PIN_D4,
        .pin_d3           = CAM_PIN_D3,
        .pin_d2           = CAM_PIN_D2,
        .pin_d1           = CAM_PIN_D1,
        .pin_d0           = CAM_PIN_D0,
        .pin_vsync        = CAM_PIN_VSYNC,
        .pin_href         = CAM_PIN_HREF,
        .pin_pclk         = CAM_PIN_PCLK,
        .xclk_freq_hz     = 10000000,
        .ledc_timer       = LEDC_TIMER_0,
        .ledc_channel     = LEDC_CHANNEL_0,
        .pixel_format     = PIXFORMAT_GRAYSCALE,
        .frame_size       = FRAMESIZE_QQVGA,   /* 160×120 */
        .jpeg_quality     = 0,
        .fb_count         = 1,
        .grab_mode        = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location      = CAMERA_FB_IN_PSRAM,
    };

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init FALHOU: %s (0x%x)",
                 esp_err_to_name(err), err);
        ESP_LOGE(TAG, "  verificar: (1) Regra R1 — imu_init() antes da câmera?");
        ESP_LOGE(TAG, "  (2) SCCB livre — I2C_NUM_0 sem uso paralelo?");
        ESP_LOGE(TAG, "  (3) PSRAM ativo? (CONFIG_SPIRAM=y)");
        return err;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor) {
        ESP_LOGI(TAG, "sensor: PID=0x%02X MID=0x%04X",
                 sensor->id.PID, sensor->id.MIDH << 8 | sensor->id.MIDL);
    }

    s_ready = true;
    ESP_LOGI(TAG, "camera_bringup init ok — QQVGA grayscale xclk=10MHz");
    return ESP_OK;
}

/* ── Captura única ────────────────────────────────────────────────── */

esp_err_t camera_bringup_capture_once(void)
{
    if (!s_ready) {
        ESP_LOGE(TAG, "capture_once: câmera não inicializada");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t t0 = (uint32_t)(esp_timer_get_time() / 1000ULL);

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGW(TAG, "capture_once: fb == NULL — câmera sem dados");
        return ESP_FAIL;
    }

    uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000ULL) - t0;

    ESP_LOGI(TAG, "frame ok — %ux%u fmt=%d len=%u bytes t=%ums",
             fb->width, fb->height, (int)fb->format,
             (unsigned)fb->len, (unsigned)elapsed);

    esp_camera_fb_return(fb);
    return ESP_OK;
}

/* ── N capturas sequenciais ───────────────────────────────────────── */

esp_err_t camera_bringup_capture_n(uint8_t n, uint32_t delay_ms)
{
    if (!s_ready) {
        ESP_LOGE(TAG, "capture_n: câmera não inicializada");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t   ok      = 0;
    uint8_t   fail    = 0;

    ESP_LOGI(TAG, "capturando %u frames (delay=%ums)...", n, (unsigned)delay_ms);

    for (uint8_t i = 0; i < n; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            ok++;
            ESP_LOGD(TAG, "  [%2u/%u] ok %ux%u len=%u",
                     (unsigned)(i + 1), (unsigned)n,
                     fb->width, fb->height, (unsigned)fb->len);
            esp_camera_fb_return(fb);
        } else {
            fail++;
            ESP_LOGW(TAG, "  [%2u/%u] FALHOU — fb == NULL", (unsigned)(i + 1), (unsigned)n);
        }

        if (delay_ms > 0 && i < (uint8_t)(n - 1)) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }

    ESP_LOGI(TAG, "resultado: %u/%u ok, %u falhas", ok, n, fail);

    return (fail == 0) ? ESP_OK : ESP_FAIL;
}

/* ── Helpers ──────────────────────────────────────────────────────── */

bool camera_bringup_is_ready(void)
{
    return s_ready;
}
