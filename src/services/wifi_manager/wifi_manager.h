#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Inicializa WiFi station.
 * Lê SSID/senha do NVS (namespace "wb_wifi", chaves "ssid"/"pass").
 * Se não houver credenciais, retorna ESP_OK mas WiFi fica inativo.
 * Reconecta automaticamente em caso de desconexão (até 5 tentativas).
 */
esp_err_t wifi_manager_init(void);

/*
 * Salva credenciais no NVS e reconecta imediatamente.
 * Chamar uma vez durante provisionamento.
 */
esp_err_t wifi_manager_set_credentials(const char *ssid, const char *password);

/* true se associado com IP válido. Thread-safe. */
bool wifi_manager_is_connected(void);

/*
 * Duty cycle para economia de energia.
 * on_ms: tempo com WiFi ligado; off_ms: tempo desligado.
 * Chamar stop para retornar ao modo contínuo.
 */
void wifi_manager_start_duty_cycle(uint32_t on_ms, uint32_t off_ms);
void wifi_manager_stop_duty_cycle(void);

#ifdef __cplusplus
}
#endif
