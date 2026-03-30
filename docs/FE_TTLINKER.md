# FE-TTLinker + SCS0009 — Referencia para o Projeto

Adaptador entre ESP32-S3 e servos SCS0009: converte UART full-duplex -> TTL half-duplex Feetech.

---

## Por que FE-TTLinker

O protocolo Feetech SCS usa barramento TTL half-duplex (um fio TX+RX, master controla direcao).
Sem o FE-TTLinker isso exige UART_MODE_RS485_HALF_DUPLEX + GPIO de controle + timing preciso.

| Sem FE-TTLinker                     | Com FE-TTLinker                |
|-------------------------------------|--------------------------------|
| UART_MODE_RS485_HALF_DUPLEX         | UART_MODE_UART (padrao)        |
| GPIO de controle de direcao         | Sem GPIO extra                 |
| Timing manual de direcao            | Automatico no hardware         |
| Codigo fragil e timing-sensitive    | UART normal, robusto           |

---

## Diagrama de conexao

```
ESP32-S3              FE-TTLinker                 Servos SCS0009

UART_SERVO_TX ───────► T (transmit input)
                                       ◄──► DATA ──── Servo ID 1
UART_SERVO_RX ◄─────── R (receive output)       ├──── Servo ID 2
                                                 └──── Servo ID N
GND ─────────────────── GND
5V ──────────────────── VCC
```

---

## Configuracao UART no ESP32-S3

```c
// scs0009_driver.c
#include "driver/uart.h"
#include "hal_init.h"

#define SERVO_UART_PORT  UART_NUM_1
#define SERVO_BAUD_RATE  1000000  // 1 Mbps — padrao de fabrica SCS0009

static void uart_init_servo(void) {
    uart_config_t cfg = {
        .baud_rate  = SERVO_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(SERVO_UART_PORT, &cfg);
    uart_set_pin(SERVO_UART_PORT,
        UART_SERVO_TX,
        UART_SERVO_RX,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(SERVO_UART_PORT, 256, 256, 0, NULL, 0);
    // SEM uart_set_mode RS485 — FE-TTLinker faz isso no hardware
}
```

---

## Protocolo Feetech SCS — estrutura do pacote

```
[0xFF] [0xFF] [ID] [Length] [Instruction] [Param1]...[ParamN] [Checksum]
```

- Length = numero de parametros + 2
- Checksum = ~(ID + Length + Instruction + sum(params)) & 0xFF

### Instructions principais

| Instruction | Valor | Uso                              |
|-------------|-------|----------------------------------|
| PING        | 0x01  | Verificar se servo responde      |
| READ        | 0x02  | Ler registrador                  |
| WRITE       | 0x03  | Escrever registrador             |
| SYNC_WRITE  | 0x83  | Escrever multiplos servos de uma vez |

### Registradores uteis do SCS0009

| Endereco | Nome             | R/W | Descricao                     |
|----------|------------------|-----|-------------------------------|
| 0x00     | Model            | R   | Modelo do servo               |
| 0x18     | Torque Enable    | R/W | 1 = ativo, 0 = livre          |
| 0x28     | Goal Position    | W   | Posicao alvo (0-1023)         |
| 0x2A     | Goal Speed       | W   | Velocidade (0-1023)           |
| 0x38     | Present Position | R   | Posicao atual (0-1023)        |
| 0x3A     | Present Speed    | R   | Velocidade atual              |
| 0x3C     | Present Load     | R   | Carga/corrente atual          |

---

## Implementacao do pacote

```c
static esp_err_t scs_write(uint8_t id, uint8_t addr, uint8_t *data, uint8_t len) {
    uint8_t pkt[16];
    pkt[0] = 0xFF;
    pkt[1] = 0xFF;
    pkt[2] = id;
    pkt[3] = len + 3;
    pkt[4] = 0x03;           // WRITE
    pkt[5] = addr;
    memcpy(&pkt[6], data, len);
    uint8_t cs = id + pkt[3] + 0x03 + addr;
    for (int i = 0; i < len; i++) cs += data[i];
    pkt[6 + len] = ~cs;
    uart_write_bytes(SERVO_UART_PORT, (char*)pkt, 7 + len);
    return ESP_OK;
}

static int scs_read_response(uint8_t *buf, size_t maxlen, uint32_t timeout_ms) {
    return uart_read_bytes(SERVO_UART_PORT, buf, maxlen, pdMS_TO_TICKS(timeout_ms));
}
```

---

## Funcoes de alto nivel

```c
// set_position: posicao 0-1023, velocidade 0-1023 (0 = maxima)
void scs0009_set_position(uint8_t id, uint16_t pos, uint16_t speed) {
    uint8_t data[4] = {
        pos & 0xFF, (pos >> 8) & 0xFF,
        speed & 0xFF, (speed >> 8) & 0xFF
    };
    scs_write(id, 0x2A, data, 4);
}

// get_position: retorna posicao lida, -1 em timeout
int16_t scs0009_get_position(uint8_t id) {
    uint8_t req[8] = {0xFF, 0xFF, id, 4, 0x02, 0x38, 2, 0};
    req[7] = ~(id + 4 + 0x02 + 0x38 + 2) & 0xFF;
    uart_flush(SERVO_UART_PORT);
    uart_write_bytes(SERVO_UART_PORT, (char*)req, 8);
    uint8_t resp[8];
    int n = uart_read_bytes(SERVO_UART_PORT, resp, 8, pdMS_TO_TICKS(5));
    if (n < 7) return -1;
    return (int16_t)(resp[5] | (resp[6] << 8));
}

// get_current_ma: le Present Load (0x3C)
// Valores tipicos: idle ~50-80mA (raw 8-12), movimento ~150-250mA (raw 23-38), stall ~800-1200mA (raw 120-185)
uint16_t scs0009_get_current_ma(uint8_t id) {
    uint8_t req[8] = {0xFF, 0xFF, id, 4, 0x02, 0x3C, 2, 0};
    req[7] = ~(id + 4 + 0x02 + 0x3C + 2) & 0xFF;
    uart_flush(SERVO_UART_PORT);
    uart_write_bytes(SERVO_UART_PORT, (char*)req, 8);
    uint8_t resp[8];
    int n = uart_read_bytes(SERVO_UART_PORT, resp, 8, pdMS_TO_TICKS(5));
    if (n < 7) return 0;
    uint16_t raw = resp[5] | (resp[6] << 8);
    return raw;  // calibrar em bancada para mA real — ver nvs_defaults.h
}
```

---

## Calibracao de corrente

O registrador Present Load (0x3C) nao retorna mA diretamente.
1. Medir corrente real com multimetro em serie
2. Ler valor do registrador 0x3C no mesmo momento
3. Calcular fator: `fator = corrente_real_mA / valor_0x3C`
4. Atualizar SERVO_CURRENT_FACTOR em nvs_defaults.h

---

## Troubleshooting

| Sintoma                        | Causa provavel            | Fix                                               |
|--------------------------------|---------------------------|---------------------------------------------------|
| get_position() sempre -1       | Servo nao responde        | Verificar baud 1Mbps, conexao FE-TTLinker, 5V     |
| Posicao errada apos movimento  | Checksum incorreto        | Recalcular checksum; verificar byte de endereco   |
| Servo nao move, PING funciona  | Torque Enable = 0         | Chamar scs0009_set_torque_enable(id, true) no init|
| Resposta corrompida            | Colisao TX/RX             | Verificar que flush e feito antes de ler          |
| Todos servos no mesmo ID       | IDs nao configurados      | Usar software Feetech para atribuir IDs unicos    |

---

## Etapas afetadas

| Etapa | Impacto                                                  |
|-------|----------------------------------------------------------|
| E04   | Driver usa UART full-duplex, sem half-duplex manual      |
| E23   | MotionSafetyService chama scs0009_get_current_ma()       |
| E34   | GestureService usa scs0009_set_position() via MotionSafety|
