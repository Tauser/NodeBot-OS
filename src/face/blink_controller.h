#pragma once

/*
 * blink_controller — gerador automático de piscadas.
 *
 * Taxa base: ~15 piscadas/min (intervalo ~4 s), ajustável por energy.
 * Sequência por piscada: fechar em 80 ms → abrir em 120 ms.
 * Restaura automaticamente a expressão-alvo após cada piscada.
 *
 * Uso:
 *   blink_controller_init();          // cria task interna (Core 0, pri 5)
 *   blink_set_energy(0.8f);           // ajusta frequência
 *   blink_suppress(true);             // inibe durante saccade/transição crítica
 *   blink_suppress(false);            // reativa
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cria a BlinkTask (FreeRTOS). Deve ser chamado após face_engine_start_task().
 */
void blink_controller_init(void);

/*
 * Energia do robô: 0.0 = sonolento (intervalo longo ~5 s),
 *                  1.0 = alerta    (intervalo curto ~2.5 s).
 * Thread-safe (escrita atômica).
 */
void blink_set_energy(float energy);

/*
 * Suprime a próxima piscada (true) ou reativa (false).
 * Chamar antes de transições críticas de expressão (saccade).
 */
void blink_suppress(bool suppress);

#ifdef __cplusplus
}
#endif
