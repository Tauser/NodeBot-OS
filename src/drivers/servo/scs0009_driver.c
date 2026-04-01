#include "scs0009_driver.h"
#include "hal_init.h"

#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "scs0009";

/* ── Registradores SCS ────────────────────────────────────────────────────── */
#define REG_TORQUE_ENABLE    0x28
#define REG_GOAL_POSITION_L  0x2A   /* 2 bytes: L, H */
#define REG_PRESENT_POS_L    0x38   /* 2 bytes: L, H */
#define REG_PRESENT_LOAD_L   0x3C   /* 2 bytes: L, H — proxy de corrente */

/* ── Instruções ───────────────────────────────────────────────────────────── */
#define INST_READ    0x02
#define INST_WRITE   0x03

/* ── Parâmetros de conversão ──────────────────────────────────────────────── */
#define CENTER_POS        512u
#define POS_MAX           1023u
#define DEG_PER_UNIT      0.2929f   /* 300° / 1024 unidades */
#define STALL_CURRENT_MA  900u
#define OVERCURRENT_MA    900u

/* ── UART ─────────────────────────────────────────────────────────────────── */
#define UART_RX_BUF   256
#define UART_TX_BUF   128
#define RESP_TIMEOUT_MS 10   /* resposta do servo em ~1-2ms a 1Mbps */

static SemaphoreHandle_t s_mutex = NULL;
static bool s_ready = false;

/* ── Protocolo ────────────────────────────────────────────────────────────── */

static uint8_t checksum(uint8_t id, uint8_t len, uint8_t inst,
                         const uint8_t *p, uint8_t n)
{
    uint32_t s = id + len + inst;
    for (uint8_t i = 0; i < n; i++) s += p[i];
    return (uint8_t)(~s & 0xFF);
}

/*
 * Envia pacote WRITE e aguarda ACK (response sem dados, só error byte).
 * Deve ser chamado COM mutex já adquirido.
 */
static esp_err_t _write_reg(uint8_t id, uint8_t addr,
                              const uint8_t *data, uint8_t n)
{
    /* FF FF ID LEN 03 ADDR DATA... CHK */
    uint8_t pkt[20];
    uint8_t n_params = n + 1u;         /* addr + dados */
    uint8_t len      = n_params + 2u;  /* n_params + inst + chk */
    uint8_t pkt_len  = (uint8_t)(2u + 1u + 1u + 1u + n_params + 1u); /* FF FF ID LEN INST params CHK */

    pkt[0] = 0xFF;
    pkt[1] = 0xFF;
    pkt[2] = id;
    pkt[3] = len;
    pkt[4] = INST_WRITE;
    pkt[5] = addr;
    memcpy(&pkt[6], data, n);

    uint8_t params_for_chk[16];
    params_for_chk[0] = addr;
    memcpy(&params_for_chk[1], data, n);
    pkt[6 + n] = checksum(id, len, INST_WRITE, params_for_chk, n_params);

    uart_flush_input(HAL_UART1_PORT);
    int sent = uart_write_bytes(HAL_UART1_PORT, pkt, pkt_len);
    if (sent != (int)pkt_len) return ESP_FAIL;

    /* SCS0009 envia status packet de 6 bytes após escrita */
    uint8_t resp[6];
    int got = uart_read_bytes(HAL_UART1_PORT, resp, sizeof(resp),
                              pdMS_TO_TICKS(RESP_TIMEOUT_MS));
    /* Toleramos timeout no ACK (write-only ok para movimento) */
    (void)got;
    return ESP_OK;
}

/*
 * Envia pacote READ e retorna n bytes em out.
 * Deve ser chamado COM mutex já adquirido.
 */
static esp_err_t _read_reg(uint8_t id, uint8_t addr, uint8_t n, uint8_t *out)
{
    /* FF FF ID 04 02 ADDR N CHK */
    uint8_t params[2] = { addr, n };
    uint8_t len = 4u;
    uint8_t pkt[8];
    pkt[0] = 0xFF;
    pkt[1] = 0xFF;
    pkt[2] = id;
    pkt[3] = len;
    pkt[4] = INST_READ;
    pkt[5] = addr;
    pkt[6] = n;
    pkt[7] = checksum(id, len, INST_READ, params, 2);

    uart_flush_input(HAL_UART1_PORT);
    uart_write_bytes(HAL_UART1_PORT, pkt, sizeof(pkt));

    /* Resposta: FF FF ID (n+2) ERR DATA... CHK */
    uint8_t resp[20] = {0};
    uint8_t expected = (uint8_t)(n + 6u);
    int got = uart_read_bytes(HAL_UART1_PORT, resp, expected,
                              pdMS_TO_TICKS(RESP_TIMEOUT_MS));
    if (got < (int)expected)              return ESP_ERR_TIMEOUT;
    if (resp[0] != 0xFF || resp[1] != 0xFF) return ESP_ERR_INVALID_RESPONSE;
    if (resp[2] != id)                    return ESP_ERR_INVALID_RESPONSE;
    if (resp[4] != 0x00)                  return ESP_FAIL; /* error byte */

    memcpy(out, &resp[5], n);
    return ESP_OK;
}

/* ── API pública ──────────────────────────────────────────────────────────── */

esp_err_t scs0009_init(void)
{
    uart_config_t cfg = {
        .baud_rate  = HAL_UART1_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(HAL_UART1_PORT,
                                         UART_RX_BUF, UART_TX_BUF,
                                         0, NULL, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(HAL_UART1_PORT, &cfg);
    if (err != ESP_OK) return err;

    err = uart_set_pin(HAL_UART1_PORT,
                       HAL_UART1_TX, HAL_UART1_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    /* Mutex com priority inheritance — evita inversão entre P22/P12 */
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    s_ready = true;
    ESP_LOGI(TAG, "ok — UART%d TX=GPIO%d RX=GPIO%d %"PRIu32"bps",
             HAL_UART1_PORT, HAL_UART1_TX, HAL_UART1_RX,
             (uint32_t)HAL_UART1_BAUD);
    return ESP_OK;
}

esp_err_t scs0009_set_position(uint8_t id, uint16_t pos, uint16_t speed)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (pos   > POS_MAX) pos   = POS_MAX;
    if (speed > POS_MAX) speed = POS_MAX;

    /* Escreve posição e velocidade em bloco único (addr 0x2A, 4 bytes) */
    uint8_t data[4] = {
        (uint8_t)(pos   & 0xFF), (uint8_t)(pos   >> 8),
        (uint8_t)(speed & 0xFF), (uint8_t)(speed >> 8),
    };
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = _write_reg(id, REG_GOAL_POSITION_L, data, 4);
    xSemaphoreGive(s_mutex);
    return err;
}

int16_t scs0009_get_position(uint8_t id)
{
    if (!s_ready) return -1;
    uint8_t raw[2];
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = _read_reg(id, REG_PRESENT_POS_L, 2, raw);
    xSemaphoreGive(s_mutex);
    if (err != ESP_OK) return -1;
    return (int16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
}

int scs0009_get_current_ma(uint8_t id)
{
    if (!s_ready) return 0;
    uint8_t raw[2];
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t err = _read_reg(id, REG_PRESENT_LOAD_L, 2, raw);
    if (err != ESP_OK) {
        xSemaphoreGive(s_mutex);
        return 0;
    }

    /* Bits [9:0] = magnitude do load; bit 10 = direção (ignorado aqui) */
    uint16_t load = (uint16_t)raw[0] | ((uint16_t)(raw[1] & 0x03) << 8);
    int current_ma = (int)((uint32_t)load * STALL_CURRENT_MA / 1023u);

    if (current_ma > (int)OVERCURRENT_MA) {
        ESP_LOGW(TAG, "overcurrent id=%u %dmA — torque off", id, current_ma);
        uint8_t off = 0;
        _write_reg(id, REG_TORQUE_ENABLE, &off, 1);  /* mutex já adquirido */
    }

    xSemaphoreGive(s_mutex);
    return current_ma;
}

void scs0009_set_torque_enable(uint8_t id, bool enable)
{
    if (!s_ready) return;
    uint8_t val = enable ? 1u : 0u;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    _write_reg(id, REG_TORQUE_ENABLE, &val, 1);
    xSemaphoreGive(s_mutex);
}

void scs0009_write_pos_deg(uint8_t id, float pos_deg)
{
    if (!s_ready) return;
    int32_t pos = (int32_t)CENTER_POS + (int32_t)(pos_deg / DEG_PER_UNIT);
    if (pos < 0)             pos = 0;
    if (pos > (int32_t)POS_MAX) pos = (int32_t)POS_MAX;
    scs0009_set_position(id, (uint16_t)pos, 512u);  /* speed 512 ≈ 50% */
}
