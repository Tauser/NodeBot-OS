#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MotionSafetyService — E23
 *
 * Task de maior prioridade da aplicação: Core 1, P22, tick 5 ms.
 *
 * A cada tick:
 *   1. Lê corrente de cada servo via get_current_ma().
 *   2. Se >800 mA acumulado por >80 ms: desativa torque + publica EVT_SERVO_BLOCKED.
 *   3. Verifica heartbeat: se não recebido em 500 ms → para todos os servos.
 *
 * Fail-safe: estado parado (torque off), não "manter última posição".
 *
 * RESTRIÇÕES ABSOLUTAS desta task:
 *   - Sem delay() bloqueante
 *   - Sem malloc()
 *   - Sem I/O de SD
 *   - Sem I2C nesta task
 */

/*
 * Inicializa o serviço e cria MotionSafetyTask (Core 1, P22).
 * Deve ser chamado após event_bus_init().
 */
void motion_safety_init(void);

/*
 * Alimenta o heartbeat. Deve ser chamado pelo BehaviorLoop antes de
 * qualquer outra operação, a cada iteração (~100 ms).
 * Thread-safe.
 */
void motion_safety_feed_heartbeat(void);

/*
 * Para todos os servos imediatamente e publica EVT_SERVO_BLOCKED.
 * Pode ser chamado de qualquer contexto. Thread-safe.
 */
void motion_safety_emergency_stop(void);

/*
 * Retorna true se o sistema está seguro para movimento de servos
 * (sem bloqueio ativo, heartbeat vivo).
 * Thread-safe.
 */
bool motion_safety_is_safe(void);

#ifdef __cplusplus
}
#endif
