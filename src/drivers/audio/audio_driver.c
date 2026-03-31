#include "audio_driver.h"
#include "hal_init.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_err.h"

#include <math.h>
#include <string.h>

static const char *TAG = "audio";

/* ── Configuração ────────────────────────────────────────────────────────── */
#define SAMPLE_RATE   16000u
#define DMA_BUFFERS   4
#define DMA_FRAMES    256

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ── Handles I2S ─────────────────────────────────────────────────────────── */
static i2s_chan_handle_t s_tx = NULL;
static i2s_chan_handle_t s_rx = NULL;

/* ── Estado ──────────────────────────────────────────────────────────────── */
static uint8_t           s_vol = 80;
static int32_t           s_raw[DMA_FRAMES * 2];
static SemaphoreHandle_t s_play_mutex = NULL;   /* serializa audio_play_pcm */

/* ── audio_init ──────────────────────────────────────────────────────────── */
/*
 * Cria TX e RX no mesmo I2S_NUM_0 com uma única chamada i2s_new_channel().
 * Isso garante que apenas um master gera BCLK/WS em GPIO41/42.
 */
void audio_init(void)
{
    /* 1. Aloca TX e RX juntos — full-duplex, um único master */
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.dma_desc_num  = DMA_BUFFERS;
    chan.dma_frame_num = DMA_FRAMES;
    ESP_ERROR_CHECK(i2s_new_channel(&chan, &s_tx, &s_rx));

    /* 2. Configura TX — MAX98357A: 16-bit dados, slot 32-bit, estéreo */
    i2s_std_config_t tx_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = HAL_I2S_SCK,         /* GPIO41 */
            .ws   = HAL_I2S_WS,          /* GPIO42 */
            .dout = HAL_I2S_AMP_DIN,     /* GPIO1  */
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0 },
        },
    };
    /* Slot 32-bit para coincidir com o clock do INMP441 (32-bit frame) */
    tx_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &tx_cfg));

    /* 3. Configura RX — INMP441: 32-bit estéreo (L/R), 16 kHz */
    i2s_std_config_t rx_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = HAL_I2S_SCK,         /* GPIO41 — compartilhado com TX */
            .ws   = HAL_I2S_WS,          /* GPIO42 — compartilhado com TX */
            .dout = I2S_GPIO_UNUSED,
            .din  = HAL_I2S_MIC_SD,      /* GPIO14 */
            .invert_flags = { 0 },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &rx_cfg));

    /* 4. Habilita ambos */
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));

    s_play_mutex = xSemaphoreCreateMutex();

    ESP_LOGI(TAG, "I2S0 full-duplex OK — %u Hz  TX:GPIO%d  RX:GPIO%d  BCLK:GPIO%d",
             SAMPLE_RATE, HAL_I2S_AMP_DIN, HAL_I2S_MIC_SD, HAL_I2S_SCK);
}

/* ── audio_mic_read ──────────────────────────────────────────────────────── */
size_t audio_mic_read(int16_t *buf, size_t samples)
{
    if (samples > DMA_FRAMES) samples = DMA_FRAMES;

    size_t bytes_read = 0;
    esp_err_t err = i2s_channel_read(s_rx, s_raw,
                                     samples * sizeof(int32_t) * 2,
                                     &bytes_read, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) return 0;

    size_t frames = bytes_read / (sizeof(int32_t) * 2);
    for (size_t i = 0; i < frames; i++) {
        int16_t l = (int16_t)(s_raw[i * 2]     >> 16);
        int16_t r = (int16_t)(s_raw[i * 2 + 1] >> 16);
        /* pega o canal com maior magnitude (INMP441: L/R=GND ou VCC) */
        buf[i] = (abs(l) >= abs(r)) ? l : r;
    }
    return frames;
}

/* ── audio_play_pcm ──────────────────────────────────────────────────────── */
void audio_play_pcm(const int16_t *buf, size_t samples)
{
    if (s_play_mutex) xSemaphoreTake(s_play_mutex, portMAX_DELAY);
    int16_t tmp[DMA_FRAMES * 2];
    size_t  off = 0;

    while (off < samples) {
        size_t chunk = (samples - off < DMA_FRAMES) ? (samples - off) : DMA_FRAMES;

        for (size_t i = 0; i < chunk; i++) {
            int16_t s = (s_vol == 100)
                        ? buf[off + i]
                        : (int16_t)((int32_t)buf[off + i] * s_vol / 100);
            tmp[i * 2]     = s;   /* L */
            tmp[i * 2 + 1] = s;   /* R */
        }

        size_t bw;
        i2s_channel_write(s_tx, tmp, chunk * 2 * sizeof(int16_t), &bw, portMAX_DELAY);
        off += chunk;
    }

    /* Flush DMA — evita crackling residual */
    memset(tmp, 0, sizeof(tmp));
    for (int i = 0; i < DMA_BUFFERS; i++) {
        size_t bw;
        i2s_channel_write(s_tx, tmp, sizeof(tmp), &bw, portMAX_DELAY);
    }
    if (s_play_mutex) xSemaphoreGive(s_play_mutex);
}

/* ── audio_set_volume ────────────────────────────────────────────────────── */
void audio_set_volume(uint8_t vol_pct)
{
    s_vol = (vol_pct > 100u) ? 100u : vol_pct;
}

/* ── audio_generate_beep ─────────────────────────────────────────────────── */
size_t audio_generate_beep(uint16_t freq_hz, uint32_t duration_ms, int16_t *out_buf)
{
    size_t n         = (size_t)((uint64_t)SAMPLE_RATE * duration_ms / 1000u);
    float  phase_inc = 2.0f * (float)M_PI * (float)freq_hz / (float)SAMPLE_RATE;

    for (size_t i = 0; i < n; i++) {
        out_buf[i] = (int16_t)(sinf(phase_inc * (float)i) * 16000.0f);
    }
    return n;
}
