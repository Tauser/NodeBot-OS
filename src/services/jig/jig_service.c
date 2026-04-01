#include "jig_service.h"

#ifdef JIG_BUILD

#include "face_engine.h"
#include "motion_safety_service.h"
#include "audio_capture.h"
#include "audio_feedback.h"
#include "sd_driver.h"
#include "state_vector.h"
#include "imu_service.h"
#include "ws2812_driver.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "jig";

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Todas as respostas vão para stdout (USB CDC / UART0 console) */
#define JIG_PRINT(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

/* ── Testes individuais ───────────────────────────────────────────────────── */

static void test_display(void)
{
    face_params_t p = {0};
    face_engine_get_target(&p);
    JIG_PRINT("DISPLAY_OK open_l=%.2f open_r=%.2f", (double)p.open_l, (double)p.open_r);
}

static void test_servo(void)
{
    if (motion_safety_is_safe()) {
        JIG_PRINT("SERVO_OK no_overcurrent");
    } else {
        JIG_PRINT("SERVO_FAIL: motion_safety em emergency_stop");
    }
}

static void test_mic(void)
{
    int16_t buf[512];
    size_t got = audio_capture_read(buf, 512);
    if (got == 0) {
        JIG_PRINT("MIC_FAIL: audio_capture_read retornou 0 amostras");
        return;
    }
    float sum = 0.0f;
    for (size_t i = 0; i < got; i++) sum += (float)buf[i] * (float)buf[i];
    float rms = sqrtf(sum / (float)got);
    if (rms > 50.0f) {
        JIG_PRINT("MIC_OK rms=%.0f", (double)rms);
    } else {
        JIG_PRINT("MIC_FAIL: rms=%.0f muito baixo (mic mudo ou desconectado)", (double)rms);
    }
}

static void test_speaker(void)
{
    audio_feedback_play(SOUND_BEEP_ACK);
    vTaskDelay(pdMS_TO_TICKS(350));
    JIG_PRINT("SPEAKER_OK");
}

static void test_sd(void)
{
    const char *path = "/sdcard/.jig_test";
    const char *data = "jig_ok";
    if (sd_write_file(path, data, strlen(data)) != ESP_OK) {
        JIG_PRINT("SD_FAIL: erro ao escrever %s", path);
        return;
    }
    char buf[16] = {0};
    size_t n = sd_read_file(path, buf, sizeof(buf) - 1);
    remove(path);
    if (n > 0 && strncmp(buf, data, strlen(data)) == 0) {
        JIG_PRINT("SD_OK bytes=%u", (unsigned)n);
    } else {
        JIG_PRINT("SD_FAIL: conteúdo lido diferente do escrito");
    }
}

static void test_battery(void)
{
    float pct = g_state.battery_pct;
    if (pct >= 0.0f && pct <= 100.0f) {
        JIG_PRINT("BATTERY_OK pct=%.1f", (double)pct);
    } else {
        JIG_PRINT("BATTERY_FAIL: valor inválido pct=%.1f", (double)pct);
    }
}

static void test_imu(void)
{
    float tilt = imu_service_get_tilt_deg();
    bool upright = imu_service_is_upright();
    if (tilt >= 0.0f && tilt < 180.0f) {
        JIG_PRINT("IMU_OK tilt=%.1f upright=%d", (double)tilt, (int)upright);
    } else {
        JIG_PRINT("IMU_FAIL: tilt=%.1f fora do intervalo esperado", (double)tilt);
    }
}

static void test_led(void)
{
    /* Verde 300ms, depois restaura estado normal */
    ws2812_set_pixel(0, 0, 180, 0);
    ws2812_set_pixel(1, 0, 180, 0);
    vTaskDelay(pdMS_TO_TICKS(300));
    ws2812_set_state(LED_STATE_NORMAL);
    JIG_PRINT("LED_OK");
}

static void test_wifi(void)
{
    /* Scan passivo: não precisa de credenciais, só verifica o hardware RF */
    wifi_scan_config_t cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = false,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    esp_err_t err = esp_wifi_scan_start(&cfg, true);  /* blocking */
    if (err != ESP_OK) {
        JIG_PRINT("WIFI_FAIL: esp_wifi_scan_start err=0x%x", (unsigned)err);
        return;
    }

    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    esp_wifi_scan_stop();

    if (count > 0) {
        JIG_PRINT("WIFI_OK networks=%u", (unsigned)count);
    } else {
        JIG_PRINT("WIFI_FAIL: nenhuma rede encontrada (antena ou RF com defeito?)");
    }
}

/* ── Dispatcher ───────────────────────────────────────────────────────────── */

typedef struct {
    const char *cmd;
    void (*fn)(void);
} jig_cmd_t;

static const jig_cmd_t k_cmds[] = {
    { "TEST_DISPLAY", test_display  },
    { "TEST_SERVO",   test_servo    },
    { "TEST_MIC",     test_mic      },
    { "TEST_SPEAKER", test_speaker  },
    { "TEST_SD",      test_sd       },
    { "TEST_BATTERY", test_battery  },
    { "TEST_IMU",     test_imu      },
    { "TEST_LED",     test_led      },
    { "TEST_WIFI",    test_wifi     },
};
#define CMD_COUNT (sizeof(k_cmds) / sizeof(k_cmds[0]))

static void dispatch(const char *line)
{
    for (size_t i = 0; i < CMD_COUNT; i++) {
        if (strcmp(line, k_cmds[i].cmd) == 0) {
            k_cmds[i].fn();
            fflush(stdout);
            return;
        }
    }
    /* Comando desconhecido — não travar */
    JIG_PRINT("UNKNOWN_CMD: %s", line);
    fflush(stdout);
}

/* ── Task ─────────────────────────────────────────────────────────────────── */

static void jig_task(void *arg)
{
    (void)arg;
    char line[64];

    while (true) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        /* Remove \r\n */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        dispatch(line);
    }
}

/* ── Init ─────────────────────────────────────────────────────────────────── */

esp_err_t jig_service_init(void)
{
    /* Sinaliza ao jig_test.py que o boot completou */
    JIG_PRINT("BOOT_OK");
    fflush(stdout);

    BaseType_t ret = xTaskCreatePinnedToCore(
        jig_task, "jig", 4096, NULL, 5, NULL, 1
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "falha ao criar jig_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ok — aguardando comandos TEST_* no serial");
    return ESP_OK;
}

#else  /* !JIG_BUILD */

esp_err_t jig_service_init(void)
{
    return ESP_OK;  /* no-op em produção */
}

#endif /* JIG_BUILD */
