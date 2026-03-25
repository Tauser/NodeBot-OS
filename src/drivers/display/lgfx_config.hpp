#pragma once

#include <LovyanGFX.hpp>
#include "hal_init.h"

/*
 * Backlight: sem pino de controle disponível — BL conectado ao 3.3 V via
 * resistor na placa (always-on). Alterar HAL_ST7789_BL se pino for alocado.
 */
#define HAL_ST7789_BL  (-1)

class LGFX : public lgfx::LGFX_Device {

    lgfx::Panel_ST7789  _panel;
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;

public:
    LGFX(void)
    {
        /* ── SPI bus ──────────────────────────────────────────────── */
        {
            auto cfg = _bus.config();

            cfg.spi_host    = SPI2_HOST;      /* FSPI no ESP32-S3 */
            cfg.spi_mode    = 0;
            cfg.freq_write  = 40000000;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;          /* D/C em pino dedicado */
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            cfg.pin_sclk = HAL_SPI_SCK;       /* GPIO47 */
            cfg.pin_mosi = HAL_SPI_MOSI;      /* GPIO21 */
            cfg.pin_miso = HAL_SPI_MISO;      /* -1 (write-only) */
            cfg.pin_dc   = HAL_ST7789_DC;     /* GPIO41 */

            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        /* ── Panel ────────────────────────────────────────────────── */
        {
            auto cfg = _panel.config();

            cfg.pin_cs   = HAL_ST7789_CS;     /* -1 (CS hardwired GND) */
            cfg.pin_rst  = HAL_ST7789_RST;    /* -1 (SWRESET via cmd) */
            cfg.pin_busy = -1;

            cfg.panel_width   = 240;
            cfg.panel_height  = 320;
            cfg.memory_width  = 240;
            cfg.memory_height = 320;

            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 0;

            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;     /* MISO não conectado */
            cfg.invert           = true;      /* ST7789 precisa de inversão */
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;     /* barramento dedicado ao display */

            _panel.config(cfg);
        }

        /* ── Backlight ────────────────────────────────────────────── */
        if (HAL_ST7789_BL >= 0) {
            auto cfg = _light.config();
            cfg.pin_bl       = HAL_ST7789_BL;
            cfg.invert       = false;
            cfg.freq         = 44100;
            cfg.pwm_channel  = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }

        setPanel(&_panel);
    }
};
