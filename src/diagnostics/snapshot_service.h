#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Inicializa o SnapshotService.
 * Cria task (Core 1, P5) que escreve snapshot JSON no SD a cada 60s.
 * Formato: /sdcard/snapshots/snap_XXXXXXXX.json
 * Captura: StateVector + heap_free + crash_count.
 */
esp_err_t snapshot_service_init(void);

/* Força um snapshot imediato (ex: antes de crash). */
void snapshot_service_flush(void);

#ifdef __cplusplus
}
#endif
