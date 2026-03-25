#include "max98357a_driver.h"
#include "hal_init.h"

#include "freertos/FreeRTOS.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_err.h"

#include <math.h>
#include <string.h>

/* ─── constantes ────────────────────────────────────────────────── */

#define SAMPLE_RATE   16000
#define DMA_BUFFERS   4
#define DMA_FRAMES    256

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static const char *TAG = "max98357a";

/* ─── estado ─────────────────────────────────────────────────────── */

static i2s_chan_handle_t s_tx;
static uint8_t           s_vol = 80;

/* ─────────────────────────────────────────────────────────────────── */

void max98357a_init(void)
{
    /* I2S_NUM_0 master TX (Full-Duplex com INMP441) */
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(HAL_I2S_PORT, I2S_ROLE_MASTER);
    chan.dma_desc_num  = DMA_BUFFERS;
    chan.dma_frame_num = DMA_FRAMES;
    ESP_ERROR_CHECK(i2s_new_channel(&chan, &s_tx, NULL));

    i2s_std_config_t std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = HAL_I2S_SPK_BCLK,   /* GPIO41 */
            .ws   = HAL_I2S_SPK_LRC,    /* GPIO42 */
            .dout = HAL_I2S_SPK_DIN,    /* GPIO1  */
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { 0 },
        },
    };
    
    /* Força o slot para 32-bit para coincidir com o clock gerado pelo INMP441 no I2S0 */
    std.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    ESP_LOGI(TAG, "MAX98357A OK — %d Hz mono 16-bit I2S0 MASTER (Full-Duplex)", SAMPLE_RATE);
}

/* ─────────────────────────────────────────────────────────────────── */

void max98357a_set_volume(uint8_t vol_pct)
{
    s_vol = (vol_pct > 100) ? 100 : vol_pct;
}

void max98357a_play_pcm(const int16_t *buf, size_t samples)
{
    /* Stereo interleaved: [L, R, L, R, ...] — cada amostra mono duplicada */
    int16_t tmp[DMA_FRAMES * 2];
    size_t  off = 0;

    while (off < samples) {
        size_t chunk = (samples - off < DMA_FRAMES) ? (samples - off) : DMA_FRAMES;

        for (size_t i = 0; i < chunk; i++) {
            int16_t s = (s_vol == 100)
                        ? buf[off + i]
                        : (int16_t)((int32_t)buf[off + i] * s_vol / 100);
            tmp[i * 2]     = s; /* L */
            tmp[i * 2 + 1] = s; /* R */
        }

        size_t bytes_written;
        i2s_channel_write(s_tx, tmp, chunk * 2 * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        off += chunk;
    }

    /* Flush: envia zeros para limpar o DMA e evitar crackling/ruído residual */
    memset(tmp, 0, sizeof(tmp));
    for (int i = 0; i < DMA_BUFFERS; i++) {
        size_t bw;
        i2s_channel_write(s_tx, tmp, sizeof(tmp), &bw, portMAX_DELAY);
    }
}

/* ─────────────────────────────────────────────────────────────────── */

size_t generate_beep(uint16_t freq_hz, uint32_t duration_ms, int16_t *out_buf)
{
    size_t  n          = (size_t)((uint64_t)SAMPLE_RATE * duration_ms / 1000);
    float   phase_inc  = 2.0f * (float)M_PI * freq_hz / (float)SAMPLE_RATE;

    for (size_t i = 0; i < n; i++) {
        /* amplitude ~50 % do máximo para evitar clipping */
        out_buf[i] = (int16_t)(sinf(phase_inc * (float)i) * 16000.0f);
    }
    return n;
}
