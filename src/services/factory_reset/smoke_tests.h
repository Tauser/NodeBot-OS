#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resultado de um item individual do smoke test */
typedef struct {
    const char *name;
    bool        pass;
    char        detail[64];   /* mensagem descritiva */
} smoke_item_t;

#define SMOKE_ITEM_COUNT 10

typedef struct {
    smoke_item_t items[SMOKE_ITEM_COUNT];
    uint8_t      passed;
    uint8_t      failed;
    uint32_t     duration_ms;
} smoke_result_t;

/*
 * Executa a suite completa de smoke tests.
 * Itens: display, servo, mic, speaker, sd, battery, imu, led, touch, wifi.
 * Cada item tem timeout de 5s. FAIL em um item não aborta os demais.
 * Duração total < 60s.
 *
 * Ao finalizar: imprime JSON no serial + LED verde (all pass) / vermelho (any fail).
 *
 * Disponível somente com -DQA_BUILD. Fora do QA_BUILD retorna resultado vazio.
 */
smoke_result_t smoke_test_run(void);

#ifdef __cplusplus
}
#endif
