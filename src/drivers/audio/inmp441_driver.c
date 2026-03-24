#include "inmp441_driver.h"
#include "hal_init.h"

#include "freertos/FreeRTOS.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_err.h"

#define DMA_BUFFERS  4
#define DMA_FRAMES   512

static const char *TAG = "inmp441";

static i2s_chan_handle_t s_rx;
static int32_t           s_raw[DMA_FRAMES * 2]; /* estéreo: 2 × 32 bits por frame */

/* ─────────────────────────────────────────────────────────────── */

void inmp441_init(void)
{
    /* Canal RX em modo master — TX não usado */
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(HAL_I2S_PORT, I2S_ROLE_MASTER);
    chan.dma_desc_num  = DMA_BUFFERS;
    chan.dma_frame_num = DMA_FRAMES;
    ESP_ERROR_CHECK(i2s_new_channel(&chan, NULL, &s_rx));

    i2s_std_config_t std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        /* Estéreo: INMP441 ocupa apenas um canal (L/R=GND→esq, L/R=VCC→dir).
         * Lemos os dois e escolhemos o que tem sinal em inmp441_read_samples. */
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = HAL_I2S_MIC_SCK,   /* GPIO44 — [C1] compartilhado UART1_RX */
            .ws   = HAL_I2S_MIC_WS,    /* GPIO42 */
            .dout = I2S_GPIO_UNUSED,
            .din  = HAL_I2S_MIC_SD,    /* GPIO14 */
            .invert_flags = { 0 },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));

    ESP_LOGI(TAG, "INMP441 OK — 16 kHz mono 32-bit  DMA:%d×%d frames",
             DMA_BUFFERS, DMA_FRAMES);
}

/* ─────────────────────────────────────────────────────────────── */

size_t inmp441_read_samples(int16_t *buf, size_t samples)
{
    if (samples > DMA_FRAMES) samples = DMA_FRAMES;

    size_t bytes_read = 0;
    /* estéreo: 2 × int32 por frame */
    esp_err_t err = i2s_channel_read(s_rx, s_raw,
                                     samples * sizeof(int32_t) * 2,
                                     &bytes_read, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) return 0;

    size_t frames = bytes_read / (sizeof(int32_t) * 2);
    for (size_t i = 0; i < frames; i++) {
        int16_t l = (int16_t)(s_raw[i * 2]     >> 16);
        int16_t r = (int16_t)(s_raw[i * 2 + 1] >> 16);
        /* pega o canal com maior magnitude — funciona para L/R=GND ou VCC */
        buf[i] = (abs(l) >= abs(r)) ? l : r;
    }
    return frames;
}
