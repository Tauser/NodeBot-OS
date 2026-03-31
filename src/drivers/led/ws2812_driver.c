#include "ws2812_driver.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

/* ─── timing WS2812B (ns) ────────────────────────────────────────── */
#define T0H_NS   400
#define T0L_NS   850
#define T1H_NS   800
#define T1L_NS   450
#define RESET_NS 55000  /* >50 µs */

#define RMT_CLK_HZ  10000000  /* 10 MHz → resolução 100 ns */
#define NS_TO_TICKS(ns) ((ns) / (1000000000 / RMT_CLK_HZ))

static const char *TAG = "ws2812";

/* ─── estado ──────────────────────────────────────────────────────── */

#define MAX_LEDS 8

static rmt_channel_handle_t  s_chan;
static rmt_encoder_handle_t  s_encoder;
static uint32_t              s_num_leds;
static uint8_t               s_buf[MAX_LEDS][3]; /* GRB */

/* ─── estado de sistema e cor emocional ───────────────────────────── */
static led_state_t           s_state      = LED_STATE_NORMAL;
static bool                  s_blink_on   = false;
static esp_timer_handle_t    s_blink_timer = NULL;

/* Cor emocional para LEDs 1+2 (usada quando state == LED_STATE_NORMAL) */
static uint8_t               s_emo_r = 0, s_emo_g = 0, s_emo_b = 0;

/* ─── encoder de bytes para símbolos RMT ─────────────────────────── */

typedef struct {
    rmt_encoder_t    base;
    rmt_encoder_t   *bytes_enc;
    rmt_encoder_t   *copy_enc;
    rmt_symbol_word_t reset_sym;
    int              state;
} ws2812_encoder_t;

static size_t ws2812_encode(rmt_encoder_t *encoder,
                            rmt_channel_handle_t chan,
                            const void *data, size_t data_size,
                            rmt_encode_state_t *out_state)
{
    ws2812_encoder_t *e = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session = RMT_ENCODING_RESET;
    size_t encoded = 0;

    if (e->state == 0) {
        encoded += e->bytes_enc->encode(e->bytes_enc, chan, data, data_size, &session);
        if (session & RMT_ENCODING_COMPLETE) e->state = 1;
        if (session & RMT_ENCODING_MEM_FULL) {
            *out_state = RMT_ENCODING_MEM_FULL;
            return encoded;
        }
    }

    if (e->state == 1) {
        encoded += e->copy_enc->encode(e->copy_enc, chan,
                                       &e->reset_sym, sizeof(e->reset_sym), &session);
        if (session & RMT_ENCODING_COMPLETE) {
            e->state = RMT_ENCODING_RESET;
            *out_state = RMT_ENCODING_COMPLETE;
        }
    }
    return encoded;
}

static esp_err_t ws2812_del(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *e = __containerof(encoder, ws2812_encoder_t, base);
    rmt_del_encoder(e->bytes_enc);
    rmt_del_encoder(e->copy_enc);
    free(e);
    return ESP_OK;
}

static esp_err_t ws2812_reset(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *e = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encoder_reset(e->bytes_enc);
    rmt_encoder_reset(e->copy_enc);
    e->state = 0;
    return ESP_OK;
}

static esp_err_t new_ws2812_encoder(rmt_encoder_handle_t *out)
{
    ws2812_encoder_t *e = calloc(1, sizeof(*e));
    if (!e) return ESP_ERR_NO_MEM;

    e->base.encode = ws2812_encode;
    e->base.del    = ws2812_del;
    e->base.reset  = ws2812_reset;

    /* Encoder de bytes: bit 1 e bit 0 */
    rmt_bytes_encoder_config_t bc = {
        .bit0 = {
            .duration0 = NS_TO_TICKS(T0H_NS), .level0 = 1,
            .duration1 = NS_TO_TICKS(T0L_NS), .level1 = 0,
        },
        .bit1 = {
            .duration0 = NS_TO_TICKS(T1H_NS), .level0 = 1,
            .duration1 = NS_TO_TICKS(T1L_NS), .level1 = 0,
        },
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&bc, &e->bytes_enc));

    /* Encoder de cópia para o símbolo de reset */
    rmt_copy_encoder_config_t cc = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&cc, &e->copy_enc));

    uint32_t reset_ticks = NS_TO_TICKS(RESET_NS);
    e->reset_sym = (rmt_symbol_word_t){
        .duration0 = reset_ticks / 2, .level0 = 0,
        .duration1 = reset_ticks / 2, .level1 = 0,
    };

    *out = &e->base;
    return ESP_OK;
}

/* ─── state machine helpers ───────────────────────────────────────── */

/* Aplica estado ao buffer GRB.
 *
 * LED_STATE_NORMAL:
 *   LED 0    → verde (sistema OK)
 *   LED 1+2  → cor emocional (s_emo_*)
 *
 * Outros estados (alertas):
 *   LED 0+1+2 → cor do alerta
 */
static void apply_state_to_buf(void)
{
    uint8_t r = 0, g = 0, b = 0;

    if (s_state == LED_STATE_NORMAL) {
        /* LED 0: indicador de sistema — verde */
        s_buf[0][0] = 30; s_buf[0][1] = 0;  s_buf[0][2] = 0;  /* GRB: G=30 */
        /* LED 1+2: cor emocional sincronizada */
        s_buf[1][0] = s_emo_g; s_buf[1][1] = s_emo_r; s_buf[1][2] = s_emo_b;
        s_buf[2][0] = s_emo_g; s_buf[2][1] = s_emo_r; s_buf[2][2] = s_emo_b;
        return;
    }

    /* Alertas: calcula cor e aplica em todos os LEDs */
    switch (s_state) {
        case LED_STATE_DEGRADED:    r = 24; g = 12;                    break;
        case LED_STATE_LISTENING:   r = 30;                            break;
        case LED_STATE_PRIVACY:     r = 20; g = 20; b = 20;            break;
        case LED_STATE_CAMERA:
            if (s_blink_on) { r = 30; }
            break;
        default: break;
    }
    for (int i = 0; i < 3; i++) {
        s_buf[i][0] = g;  /* GRB */
        s_buf[i][1] = r;
        s_buf[i][2] = b;
    }
}

static void blink_timer_cb(void *arg)
{
    (void)arg;
    s_blink_on = !s_blink_on;
    apply_state_to_buf();
    /* transmite sem bloquear (queue profundidade 4) */
    rmt_transmit_config_t tx = { .loop_count = 0 };
    rmt_transmit(s_chan, s_encoder, s_buf, s_num_leds * 3, &tx);
}

/* ─────────────────────────────────────────────────────────────────── */

void ws2812_init(gpio_num_t gpio, uint32_t num_leds)
{
    s_num_leds = (num_leds > MAX_LEDS) ? MAX_LEDS : num_leds;
    memset(s_buf, 0, sizeof(s_buf));

    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num          = gpio,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = RMT_CLK_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_chan));
    ESP_ERROR_CHECK(new_ws2812_encoder(&s_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_chan));

    ESP_LOGI(TAG, "WS2812 OK — GPIO%d  %"PRIu32" LEDs", gpio, s_num_leds);
}

void ws2812_set_pixel(uint32_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx >= s_num_leds) return;
    s_buf[idx][0] = g; /* WS2812 usa ordem GRB */
    s_buf[idx][1] = r;
    s_buf[idx][2] = b;
}

void ws2812_show(void)
{
    /* Estado de sistema sempre prevalece sobre LED 0 */
    apply_state_to_buf();
    rmt_transmit_config_t tx = { .loop_count = 0 };
    ESP_ERROR_CHECK(rmt_transmit(s_chan, s_encoder,
                                 s_buf, s_num_leds * 3, &tx));
    rmt_tx_wait_all_done(s_chan, pdMS_TO_TICKS(100));
}

void ws2812_set_state(led_state_t state)
{
    /* Para blink anterior se existir */
    if (s_blink_timer) {
        esp_timer_stop(s_blink_timer);
        if (state != LED_STATE_CAMERA) {
            esp_timer_delete(s_blink_timer);
            s_blink_timer = NULL;
        }
    }

    s_state   = state;
    s_blink_on = false;

    if (state == LED_STATE_CAMERA) {
        if (!s_blink_timer) {
            esp_timer_create_args_t args = {
                .callback = blink_timer_cb,
                .name     = "led_blink",
            };
            if (esp_timer_create(&args, &s_blink_timer) == ESP_OK) {
                esp_timer_start_periodic(s_blink_timer, 500000); /* 2 Hz */
            }
        } else {
            esp_timer_start_periodic(s_blink_timer, 500000);
        }
    }

    ws2812_show();
}

led_state_t ws2812_get_state(void)
{
    return s_state;
}

void ws2812_set_emotion_color(uint8_t r, uint8_t g, uint8_t b)
{
    s_emo_r = r;
    s_emo_g = g;
    s_emo_b = b;
    /* Aplica imediatamente só se não houver alerta ativo */
    if (s_state == LED_STATE_NORMAL) {
        ws2812_show();
    }
}
