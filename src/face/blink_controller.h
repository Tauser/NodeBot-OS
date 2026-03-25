#pragma once

/*
 * blink_controller — piscadas automáticas estilo EMO, 3 keyframes.
 *
 * Sequência por piscada:
 *   KF1  ESPREMENDO  70 ms  — cantos bl/br fecham, olho desce
 *   KF2  FECHADO     30 ms  — hold quase completamente fechado
 *   KF3  ABRINDO    110 ms  — restaura expressão salva antes do blink
 *
 * Taxa automática: rand(2500, 5000) ms entre blinks, ajustável por energy.
 *
 * Uso:
 *   blink_controller_init();       // cria BlinkTask (Core 0, pri 5)
 *   blink_controller_trigger();    // disparo imediato (one-shot)
 *   blink_set_energy(0.8f);        // 0.0=sonolento(~5s)  1.0=alerta(~2.5s)
 *   blink_suppress(true);          // cancela KF1 se ainda em curso, abre olho
 *   blink_suppress(false);         // reativa blinks automáticos
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
 * Thread-safe; retorna imediatamente (executa em task one-shot).
 */
void blink_controller_trigger(void);

/*
 * Energia 0.0–1.0: controla intervalo entre blinks automáticos.
 * Thread-safe (escrita atômica em volatile).
 */
void blink_set_energy(float energy);

/*
 * true  → cancela blink se ainda no KF1 e suprime o próximo ciclo automático.
 * false → reativa blinks automáticos.
 */
void blink_suppress(bool suppress);

#ifdef __cplusplus
}
#endif
