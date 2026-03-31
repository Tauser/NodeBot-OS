#include "camera_service.h"
#include "event_bus.h"
#include "hal_init.h"
#include "esp_camera.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "cam"

/* ── Dimensões QQVGA grayscale ─────────────────────────────────────── */
#define CAM_WIDTH   160
#define CAM_HEIGHT  120
#define CAM_PIXELS  (CAM_WIDTH * CAM_HEIGHT)   /* 19 200 bytes */

/* Diff threshold para tier EMPTY→OCCUPIED (em [0,255]) */
#define PRESENCE_THRESHOLD_LOW   10   /* → OCCUPIED quando diff > 10  */
#define PRESENCE_THRESHOLD_HIGH   6   /* → EMPTY   quando diff < 6    */

/* ── Pinos OV2640 (fixos de placa) ────────────────────────────────── */
#define CAM_PIN_XCLK    15
#define CAM_PIN_PCLK    13
#define CAM_PIN_VSYNC    6
#define CAM_PIN_HREF     7
#define CAM_PIN_SIOD     HAL_I2C_SDA   /* = 4, SCCB compartilhado     */
#define CAM_PIN_SIOC     HAL_I2C_SCL   /* = 5, SCCB compartilhado     */
#define CAM_PIN_D0      11
#define CAM_PIN_D1       9
#define CAM_PIN_D2       8
#define CAM_PIN_D3      10
#define CAM_PIN_D4      12
#define CAM_PIN_D5      18
#define CAM_PIN_D6      17
#define CAM_PIN_D7      16
#define CAM_PIN_PWDN    (-1)
#define CAM_PIN_RESET   (-1)

/* ── Estado interno ───────────────────────────────────────────────── */
static bool           s_ready = false;
static TaskHandle_t   s_task_handle;

/* Double buffer em PSRAM: prev e curr alternam a cada captura */
static uint8_t       *s_buf_a;
static uint8_t       *s_buf_b;
static uint8_t       *s_buf_prev;
static uint8_t       *s_buf_curr;

static bool           s_presence;   /* último estado detectado */

/* ── Detecção de presença ─────────────────────────────────────────── */

uint8_t camera_service_get_frame_diff(const uint8_t *prev, const uint8_t *curr)
{
    if (!prev || !curr) return 0;
    uint32_t sum = 0;
    uint32_t n   = 0;
    for (int i = 0; i < CAM_PIXELS; i += 4) {
        int d = (int)curr[i] - (int)prev[i];
        sum += (uint32_t)(d < 0 ? -d : d);
        n++;
    }
    return (uint8_t)(sum / n);
}

static void update_presence(uint8_t diff)
{
    bool new_presence = s_presence;

    if (!s_presence && diff > PRESENCE_THRESHOLD_LOW) {
        new_presence = true;
    } else if (s_presence && diff < PRESENCE_THRESHOLD_HIGH) {
        new_presence = false;
    }

    if (new_presence != s_presence) {
        s_presence = new_presence;
        uint8_t confidence = (uint8_t)((diff * 100u) / 255u);
        presence_event_t ev = { .present = new_presence, .confidence = confidence };
        event_bus_publish(EVT_PRESENCE_DETECTED, &ev, sizeof(ev), EVENT_PRIO_BEHAVIOR);
        ESP_LOGI(TAG, "presença: %s (diff=%u conf=%u%%)",
                 new_presence ? "OCUPADO" : "VAZIO", diff, confidence);
    }
}

/* ── Task de captura (Core 0) ────────────────────────────────────── */

static void camera_capture_task(void *arg)
{
    (void)arg;
    uint32_t notif;

    for (;;) {
        /* Aguarda notificação de camera_service_request_frame() */
        xTaskNotifyWait(0, ULONG_MAX, &notif, portMAX_DELAY);

        if (!s_ready) continue;

        /* Captura — I2C não deve ser acessado durante este período */
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "esp_camera_fb_get falhou");
            continue;
        }

        /* Copia para buffer PSRAM próprio e alterna double buffer */
        size_t copy_len = fb->len < CAM_PIXELS ? fb->len : CAM_PIXELS;
        memcpy(s_buf_curr, fb->buf, copy_len);
        esp_camera_fb_return(fb);

        /* Diff para presença */
        uint8_t diff = camera_service_get_frame_diff(s_buf_prev, s_buf_curr);
        update_presence(diff);

        /* Publica frame */
        camera_frame_event_t ev = {
            .frame_buf    = s_buf_curr,
            .width        = CAM_WIDTH,
            .height       = CAM_HEIGHT,
            .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL),
        };
        event_bus_publish(EVT_CAMERA_FRAME, &ev, sizeof(ev), EVENT_PRIO_BEHAVIOR);

        /* Alterna buffers: curr vira prev para o próximo ciclo */
        uint8_t *tmp = s_buf_prev;
        s_buf_prev   = s_buf_curr;
        s_buf_curr   = tmp;
    }
}

/* ── Init ─────────────────────────────────────────────────────────── */

esp_err_t camera_service_init(void)
{
    /* Aloca dois frame buffers grayscale em PSRAM */
    s_buf_a = heap_caps_malloc(CAM_PIXELS, MALLOC_CAP_SPIRAM);
    s_buf_b = heap_caps_malloc(CAM_PIXELS, MALLOC_CAP_SPIRAM);
    if (!s_buf_a || !s_buf_b) {
        ESP_LOGE(TAG, "PSRAM insuficiente para frame buffers");
        return ESP_ERR_NO_MEM;
    }
    memset(s_buf_a, 0, CAM_PIXELS);
    memset(s_buf_b, 0, CAM_PIXELS);
    s_buf_prev = s_buf_a;
    s_buf_curr = s_buf_b;
    s_presence = false;

    /* Configuração OV2640 */
    camera_config_t cfg = {
        .pin_pwdn   = CAM_PIN_PWDN,
        .pin_reset  = CAM_PIN_RESET,
        .pin_xclk   = CAM_PIN_XCLK,
        .pin_sccb_sda  = -1,            /* ignorado — sccb_i2c_port usado */
        .pin_sccb_scl  = -1,
        .sccb_i2c_port = HAL_I2C_PORT, /* reutiliza I2C_NUM_0 (i2c_bus)  */
        .pin_d7     = CAM_PIN_D7,
        .pin_d6     = CAM_PIN_D6,
        .pin_d5     = CAM_PIN_D5,
        .pin_d4     = CAM_PIN_D4,
        .pin_d3     = CAM_PIN_D3,
        .pin_d2     = CAM_PIN_D2,
        .pin_d1     = CAM_PIN_D1,
        .pin_d0     = CAM_PIN_D0,
        .pin_vsync  = CAM_PIN_VSYNC,
        .pin_href   = CAM_PIN_HREF,
        .pin_pclk   = CAM_PIN_PCLK,
        .xclk_freq_hz    = 10000000,
        .ledc_timer      = LEDC_TIMER_0,
        .ledc_channel    = LEDC_CHANNEL_0,
        .pixel_format    = PIXFORMAT_GRAYSCALE,
        .frame_size      = FRAMESIZE_QQVGA,
        .jpeg_quality    = 0,   /* sem uso para grayscale */
        .fb_count        = 1,
        .grab_mode       = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location     = CAMERA_FB_IN_PSRAM,
    };

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init: %s", esp_err_to_name(err));
        heap_caps_free(s_buf_a);
        heap_caps_free(s_buf_b);
        return err;
    }

    /* Task de captura no Core 0 */
    BaseType_t rc = xTaskCreatePinnedToCore(camera_capture_task, "CamCapture",
                                             4096, NULL, 12,
                                             &s_task_handle, 0);
    if (rc != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate falhou");
        esp_camera_deinit();
        heap_caps_free(s_buf_a);
        heap_caps_free(s_buf_b);
        return ESP_FAIL;
    }

    s_ready = true;
    ESP_LOGI(TAG, "init ok — OV2640 QQVGA grayscale, PSRAM=%u bytes",
             (unsigned)(2 * CAM_PIXELS));
    return ESP_OK;
}

/* ── API pública ──────────────────────────────────────────────────── */

void camera_service_request_frame(void)
{
    if (!s_ready || !s_task_handle) return;
    xTaskNotify(s_task_handle, 1, eSetValueWithOverwrite);
}

bool camera_service_is_ready(void)
{
    return s_ready;
}
