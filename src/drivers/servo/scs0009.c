#include "scs0009.h"

/*
 * Stubs fracos — substituídos pelo driver real quando implementado (E?).
 * __attribute__((weak)) garante que o linker prefere a implementação forte.
 */

__attribute__((weak))
int32_t get_current_ma(uint8_t servo_id)
{
    (void)servo_id;
    return 0;
}

__attribute__((weak))
void scs0009_set_torque_enable(bool enable)
{
    (void)enable;
}
