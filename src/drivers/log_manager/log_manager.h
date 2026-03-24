#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Dimensões ───────────────────────────────────────────────────────── */
#define LOG_BUF_SIZE        (16u * 1024u)         /* buffer circular em PSRAM  */
#define LOG_MAX_LINE        256u                   /* máx bytes por entrada     */
#define LOG_MAX_FILE_SIZE   (1u * 1024u * 1024u)  /* 1 MB — aciona rotação     */
#define LOG_SD_PATH         "/sdcard"
#define LOG_FLUSH_PERIOD_MS 10000                  /* flush periódico           */

/* ── Nível de severidade ─────────────────────────────────────────────── */
typedef enum {
    LOG_FATAL = 0,
    LOG_ERROR = 1,
    LOG_WARN  = 2,
    LOG_INFO  = 3,
    LOG_DEBUG = 4,
    LOG_LEVEL_MAX,
} log_level_t;

/**
 * Inicializa o LogManager.
 * Aloca buffer circular (LOG_BUF_SIZE) em PSRAM, com fallback para SRAM.
 * Cria a task de flush periódico (LOG_FLUSH_PERIOD_MS).
 */
esp_err_t log_init(void);

/**
 * Escreve uma entrada no buffer circular. NÃO BLOQUEANTE (<100 µs).
 * Nunca realiza I/O de SD. Se o buffer estiver cheio, a linha mais
 * antiga é descartada para abrir espaço.
 *
 * RESTRIÇÃO: service e message não devem conter '"' ou '\'.
 *
 * Formato gerado:
 *   {"l":"INFO","s":"<service>","m":"<message>","t":<ms_since_boot>}
 */
esp_err_t log_write(log_level_t level, const char *service, const char *message);

/**
 * Flush síncrono: drena o buffer circular para LOG_SD_PATH/log_0.jsonl
 * e verifica se é necessária rotação de arquivo.
 * Bloqueia até a escrita ser concluída. Chamar antes de reset/pânico.
 *
 * Rotação (se log_0.jsonl > LOG_MAX_FILE_SIZE):
 *   remove log_2.jsonl  →  log_1.jsonl → log_2.jsonl  →  log_0.jsonl → log_1.jsonl
 */
void log_flush_now(void);

/**
 * Retorna contadores acumulados desde log_init().
 * Ponteiros NULL são ignorados.
 */
void log_get_stats(uint32_t *written, uint32_t *dropped, uint32_t *flushed_bytes);

#ifdef __cplusplus
}
#endif
