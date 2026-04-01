#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Driver SCS0009 via FE-TTLinker — E04
 *
 * UART1 full-duplex (HAL_UART1_TX / HAL_UART1_RX).
 * O FE-TTLinker converte UART full-duplex → TTL half-duplex Feetech.
 * Sem controle de pino de direção — usar UART_MODE_UART padrão.
 *
 * Protocolo Feetech SCS:
 *   FF FF ID LEN INST PARAMS... CHK
 *   CHK = ~(ID + LEN + INST + sum(PARAMS)) & 0xFF
 *
 * Proteção básica: scs0009_get_current_ma() > 900 mA → torque off imediato.
 * Proteção completa fica no MotionSafetyService (E23).
 *
 * Thread-safe via mutex (FreeRTOS mutex com priority inheritance).
 * Chamar scs0009_init() no boot antes de qualquer uso.
 */

esp_err_t scs0009_init(void);

/*
 * Move servo para posição e velocidade.
 * pos:   0–1023 (512 = centro)
 * speed: 0–1023 (0 = máxima, 512 ≈ 50%)
 */
esp_err_t scs0009_set_position(uint8_t id, uint16_t pos, uint16_t speed);

/*
 * Lê posição atual.
 * Retorna -1 em timeout ou erro de comunicação.
 */
int16_t scs0009_get_position(uint8_t id);

/*
 * Lê corrente aproximada em mA (proxy via registrador Present Load).
 * Se > 900 mA: desativa torque automaticamente (proteção básica).
 * Retorna 0 em timeout.
 */
int scs0009_get_current_ma(uint8_t id);

/*
 * Habilita ou desabilita torque do servo.
 */
void scs0009_set_torque_enable(uint8_t id, bool enable);

/*
 * Converte graus → posição e envia. Centro = 0°, range ±150°.
 * Substitui o stub weak do gesture_service automaticamente.
 */
void scs0009_write_pos_deg(uint8_t id, float pos_deg);

#ifdef __cplusplus
}
#endif
