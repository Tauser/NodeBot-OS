#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * scs0009 — driver para servo SCS0009 via FE-TTLinker (UART1).
 *
 * Stub fraco: implementações __attribute__((weak)) em scs0009.c.
 * O driver real, quando implementado, substitui automaticamente via linker.
 */

/*
 * Lê a corrente instantânea do servo (mA).
 * Retorna 0 se o driver não estiver disponível ou o ID for inválido.
 * servo_id: 0 ou 1 (pan / tilt).
 */
int32_t get_current_ma(uint8_t servo_id);

/*
 * Habilita ou desabilita o torque de TODOS os servos.
 * enable=false → servos livres (sem força), usado em emergency stop.
 */
void scs0009_set_torque_enable(bool enable);

#ifdef __cplusplus
}
#endif
