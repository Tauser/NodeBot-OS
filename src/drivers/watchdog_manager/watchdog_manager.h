#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * HW WDT (interrupt watchdog): 8 s — configurado via sdkconfig.defaults.
 *   CONFIG_ESP_INT_WDT=y
 *   CONFIG_ESP_INT_WDT_TIMEOUT_MS=8000
 *
 * Task WDT: 3 s, panic=true — configurado via sdkconfig.defaults.
 *   CONFIG_ESP_TASK_WDT_EN=y
 *   CONFIG_ESP_TASK_WDT_TIMEOUT_S=3
 *   CONFIG_ESP_TASK_WDT_PANIC=y
 */
#define WDT_HW_TIMEOUT_MS    8000
#define WDT_TASK_TIMEOUT_MS  3000
#define WDT_MAX_ENTRIES      8

/**
 * Inicializa o WatchdogManager.
 * Verifica que o Task WDT está ativo com as configurações corretas
 * (definidas em sdkconfig.defaults). Não cria tarefas adicionais.
 */
esp_err_t wdt_init(void);

/**
 * Registra uma task no Task WDT com timeout_ms.
 * Cria um "user" nomeado no TWDT para a task informada.
 * task = NULL → usa a task chamadora.
 *
 * O TWDT dispara panic se wdt_feed() não for chamado dentro de
 * WDT_TASK_TIMEOUT_MS (configuração global).
 */
esp_err_t wdt_register_task(TaskHandle_t task, uint32_t timeout_ms);

/**
 * Alimenta o watchdog para a task registrada.
 * task = NULL → usa a task chamadora.
 * Deve ser chamado dentro de WDT_TASK_TIMEOUT_MS.
 *
 * @return ESP_OK           alimentado com sucesso
 *         ESP_ERR_NOT_FOUND task não registrada
 */
esp_err_t wdt_feed(TaskHandle_t task);

#ifdef __cplusplus
}
#endif
