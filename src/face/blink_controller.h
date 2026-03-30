#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * blink_controller — piscadas automáticas estilo EMO.
 *
 * Sequência por piscada:
 *   KF1  FECHANDO    80 ms
 *   KF2  FECHADO     30 ms
 *   KF3  ABRINDO    120 ms
 *
 * Taxa automática: janela base rand(2500, 5000) ms entre blinks,
 * enviesada por energy sem sair do comportamento natural.
 *
 * O blink atua como override visual de runtime sobre a face atual.
 * Não troca a expressão base nem altera a emoção alvo salva no FaceEngine.
 *
 * Uso:
 *   blink_controller_init();       // cria uma única BlinkTask
 *   blink_controller_trigger();    // agenda uma piscada imediata
 *   blink_set_energy(0.8f);        // 0.0=mais lento  1.0=mais atento
 *   blink_suppress(true);          // inibe piscadas automáticas e aborta KF1/KF2
 *   blink_suppress(false);         // reativa piscadas automáticas
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cria a BlinkTask. Deve ser chamado após face_engine_start_task().
 */
void blink_controller_init(void);

/*
 * Dispara uma piscada imediatamente, fora do ciclo automático.
 * Thread-safe; retorna imediatamente e usa a BlinkTask já existente.
 */
void blink_controller_trigger(void);

/*
 * Energia 0.0–1.0: controla intervalo entre blinks automáticos.
 * Thread-safe via seção crítica curta.
 */
void blink_set_energy(float energy);

/*
 * true  → suprime novos blinks e aborta a piscada em curso durante KF1/KF2.
 * false → reativa blinks automáticos.
 */
void blink_suppress(bool suppress);

#ifdef __cplusplus
}
#endif
