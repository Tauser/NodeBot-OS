# E04 - Bring-up: Servos SCS0009

- Status: 🔲 Não iniciada
- Complexidade: Média
- Grupo: Bring-up HW
- HW Real: SIM
- Prioridade: P1
- Risco: CRÍTICO
- Depende de: E02

## Bring-up: Servos SCS0009

Complexidade: Média
Depende de: E02
Grupo: Bring-up HW
HW Real: SIM
ID: E04
Prioridade: P1
Risco: CRÍTICO
Status: 🔲 Não iniciada

## 🔧 Status de hardware: FE-TTLinker ainda não conectado?

> **Sem problema.** Você pode implementar 100% do driver agora. O que muda é apenas o que você pode testar.
> 

| O que fazer agora (sem hardware) | O que aguardar o hardware |
| --- | --- |
| Escrever `scs0009_driver.h/.c` completo | Testar movimento real |
| Implementar montagem/desmontagem de pacotes Feetech | Medir corrente real em idle/movimento/stall |
| Escrever testes unitários com mock UART | Calibrar fator de corrente do registrador 0x3C |
| Configurar UART1 full-duplex no ESP32 | Verificar resposta real do servo |
| Implementar checagem de checksum | Confirmar baud rate do seu lote específico |
| Escrever `motion_safety_service` (E23) em paralelo | — |

**Como testar sem hardware:**

```c
// Mock UART para testes unitários
// Em vez de uart_write_bytes() real, usar um buffer de loopback
// Verificar que os pacotes montados têm o formato correto
// Verificar checksum dos pacotes gerados
```

**Quando o hardware chegar:**

1. Conectar FE-TTLinker conforme diagrama da página 🔌 FE-TTLinker
2. Executar o teste de movimento 0→512→0
3. Medir corrente com multímetro e calibrar o fator de conversão
4. Marcar os critérios de pronto que dependem de HW

---

## ⚠️ ETAPA CRÍTICA — Risco de incêndio por stall de servo com LiPo

## Objetivo

Servos SCS0009 movendo via protocolo Feetech, lidos via UART full-duplex normal. Feedback de posição e leitura de corrente funcionando. Limite básico de corrente no driver antes de qualquer outro código usar os servos.

---

## Hardware: FE-TTLinker

O projeto usa um **FE-TTLinker** entre o ESP32 e os servos.

```
ESP32-S3          FE-TTLinker         Servos SCS0009
UART_TX ─────────► TX_IN              ┐
UART_RX ◄──────── RX_OUT    TTL Bus ──┼── Servo ID 1
GND ─────────────── GND              ├── Servo ID 2
5V ──────────────── VCC              └── ...
```

**O que o FE-TTLinker faz:**

- Converte UART full-duplex (TX e RX separados) → barramento TTL half-duplex Feetech
- Gerencia automaticamente a direção do barramento (sem código de controle de direção no ESP32)
- Permite usar `uart_write_bytes()` e `uart_read_bytes()` normalmente

**O que isso significa para o firmware:**

- ✅ UART configurado como `UART_MODE_UART` padrão — sem `UART_MODE_RS485_HALF_DUPLEX`
- ✅ Sem GPIO de controle de direção (DE/RE)
- ✅ Sem `uart_set_line_inverse()` para half-duplex
- ✅ TX e RX usados simultaneamente e independentemente

---

## O que implementar

- Configurar UART1 em modo full-duplex padrão (TX = `UART_SERVO_TX`, RX = `UART_SERVO_RX` conforme `hal_init.h`)
- Escrever `scs0009_driver.h/.c`: `init`, `set_position(id, pos, speed)`, `get_position(id)`, `get_current_ma(id)`, `set_torque_enable(id, bool)`
- Implementar protocolo Feetech SCS em cima do UART normal: montar pacote → `uart_write_bytes()` → `uart_read_bytes()` com timeout
- **Limite básico de corrente NO DRIVER:** se `get_current_ma() > 900mA` → chamar `set_torque_enable(false)` imediatamente
- Testar movimento `0°→90°→0°` confirmando feedback de posição
- Documentar corrente em idle vs. movimento vs. bloqueio

## O que NÃO entra

- MotionSafetyService completo (vem na E23)
- Gestos ou movimentos integrados ao behavior
- Qualquer integração com EventBus

## Critérios de pronto

- Servo move `0°→90°` e retorna com feedback de posição correto (delta < 5°)
- Corrente em bloqueio mecânico detectada e servo parado em < 200ms pelo driver
- Corrente em idle, movimento e bloqueio documentados numericamente

## Testes mínimos

- **CRÍTICO:** bloquear servo mecanicamente por 3s → driver para em < 200ms, sem aquecimento
- Mover 10× e confirmar que posição lida bate com posição enviada (± 5°)
- Medir corrente em idle, movimento e bloqueio com multímetro

---

## ▶ Prompt Principal

```
Contexto: ESP32-S3, UART1 full-duplex padrão (sem half-duplex manual), FE-TTLinker entre ESP32 e servos SCS0009.
O FE-TTLinker converte UART full-duplex → TTL half-duplex Feetech automaticamente.
Firmware: usar UART_MODE_UART normal, sem controle de pino de direção.

Pinos conforme hal_init.h: UART_SERVO_TX, UART_SERVO_RX.
Baud rate Feetech padrão de fábrica: 1.000.000 bps.

Tarefa: scs0009_driver.h + scs0009_driver.c.

Funções obrigatórias:
  esp_err_t scs0009_init(void);
  esp_err_t scs0009_set_position(uint8_t id, uint16_t pos_0_1023, uint16_t speed);
  int16_t   scs0009_get_position(uint8_t id);       // retorna -1 em timeout
  uint16_t  scs0009_get_current_ma(uint8_t id);     // retorna 0 em timeout
  esp_err_t scs0009_set_torque_enable(uint8_t id, bool enable);

Protocolo Feetech SCS (pacote de escrita/leitura):
  Header: 0xFF 0xFF
  ID: uint8_t
  Length: uint8_t (N parâmetros + 2)
  Instruction: 0x03 (WRITE) ou 0x02 (READ)
  Parameters: endereço + dados
  Checksum: ~(ID + Length + Instruction + sum(params)) & 0xFF

Proteção básica no driver: em cada chamada a get_current_ma(), se retorno > 900 → set_torque_enable(id, false).

Config UART:
  uart_config_t cfg = {
    .baud_rate  = 1000000,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };
  // SEM: uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)
  // SEM: gpio de controle de direção

Saída: scs0009_driver.h + scs0009_driver.c + teste de movimento 0→512→0 com leitura de posição e corrente.
```

## ◎ Prompt de Revisão

```
Driver SCS0009 com FE-TTLinker da E04.
CRÍTICO verificar:
(1) UART configurado como full-duplex (UART_MODE_UART) — NÃO RS485 half-duplex?
(2) Sem gpio de controle de direção DE/RE?
(3) Proteção de corrente implementada no driver (>900mA → para)?
(4) Timeout em get_position() e get_current_ma() (não bloqueia indefinidamente)?
(5) Checksum do protocolo Feetech calculado corretamente?
Listar problemas com severidade CRÍTICO/ALTO/MÉDIO.
```

## ✎ Prompt de Correção

```
Driver SCS0009 com problema: [sintoma — ex: get_position() sempre retorna -1, ou servo não responde]
Contexto: E04, ESP32-S3, UART full-duplex, FE-TTLinker, SCS0009.
Verificar: baud rate (1Mbps padrão de fábrica), checksum, timeout de leitura (response do servo ~1–2ms após comando).
Saída: causa provável + fix.
```

## → Prompt de Continuidade

```
E04 concluída. Servos movendo via FE-TTLinker (UART full-duplex normal), feedback de posição e corrente OK, proteção básica de corrente no driver.
Próxima: E05 (bring-up do microfone INMP441).
Mostre como configurar I2S PDM para INMP441 no ESP32-S3 com ESP-IDF e como ler amostras de 32-bit em buffer circular.
```


