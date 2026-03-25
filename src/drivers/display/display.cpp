#include "display.h"
#include "lgfx_config.hpp"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "drv_display";

static LGFX _lcd;

/* ─────────────────────────────────────────────────────────────── */

void display_init(void)
{
    _lcd.init();
    _lcd.setRotation(1); /* Define a rotação final logo de cara */
    _lcd.fillScreen(TFT_BLACK);
    ESP_LOGI(TAG, "ST7789 %dx%d @ SPI2 inicializado",
             _lcd.width(), _lcd.height());
}

void display_fill_color(uint16_t color)
{
    _lcd.fillScreen(color);
}

void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    _lcd.fillRect(x, y, w, h, color);
}

void display_push_sprite(void *sprite, int16_t x, int16_t y)
{
    auto *s = static_cast<lgfx::LGFX_Sprite *>(sprite);
    s->pushSprite(&_lcd, x, y);
}

/* ─────────────────────────────────────────────────────────────── */

float display_fps_test(uint32_t duration_ms)
{
    const uint16_t colors[] = { TFT_RED, TFT_BLUE };
    uint32_t frames = 0;

    int64_t t_start = esp_timer_get_time();          /* µs */
    int64_t t_end   = t_start + (int64_t)duration_ms * 1000;

    while (esp_timer_get_time() < t_end) {
        _lcd.fillScreen(colors[frames & 1]);
        frames++;
    }

    float elapsed_s = (float)(esp_timer_get_time() - t_start) / 1e6f;
    float fps = (float)frames / elapsed_s;

    ESP_LOGI(TAG, "FPS test: %lu frames em %.2f s → %.1f fps",
             (unsigned long)frames, elapsed_s, fps);
    return fps;
}
