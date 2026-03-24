#include "log_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stdbool.h>

static const char *TAG = "log_mgr";

/* ══════════════════════════════════════════════════════════════════════
   Buffer circular em PSRAM — 16 KB estáticos
   head = próxima posição de escrita
   tail = próxima posição de leitura (flush)
   Invariante: head == tail → vazio; (head+1)%CAP == tail → cheio
   ══════════════════════════════════════════════════════════════════════ */
#define CAP  LOG_BUF_SIZE

static uint8_t     *s_buf;
static uint32_t     s_head;
static uint32_t     s_tail;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* ── flush ───────────────────────────────────────────────────────────── */
static SemaphoreHandle_t s_flush_mtx;
static TaskHandle_t      s_flush_task;

/* ── stats ───────────────────────────────────────────────────────────── */
static uint32_t s_stat_written;
static uint32_t s_stat_dropped;
static uint32_t s_stat_flushed;

/* ── strings de nível ────────────────────────────────────────────────── */
static const char * const s_lvl[LOG_LEVEL_MAX] = {
    "FATAL", "ERROR", "WARN", "INFO", "DEBUG"
};

/* ══════════════════════════════════════════════════════════════════════
   Ring buffer helpers (chamados dentro do spinlock)
   ══════════════════════════════════════════════════════════════════════ */

static inline uint32_t ring_used(void)
{
    return (s_head + CAP - s_tail) % CAP;
}

static inline uint32_t ring_free(void)
{
    return CAP - 1u - ring_used();
}

/*
 * Garante espaço livre >= len, descartando as linhas mais antigas.
 * Cada entrada termina com '\n', portanto a varredura é curta (<= LOG_MAX_LINE).
 */
static void ring_make_room(uint32_t len)
{
    while (ring_free() < len) {
        uint32_t used = ring_used();
        bool found = false;
        uint32_t i = s_tail;

        for (uint32_t j = 0; j < used; j++) {
            if (s_buf[i] == '\n') {
                s_tail = (i + 1u) % CAP;
                s_stat_dropped++;
                found = true;
                break;
            }
            i = (i + 1u) % CAP;
        }

        if (!found) {
            /* Buffer sem nenhum '\n' — limpa tudo como último recurso */
            s_tail = s_head;
            s_stat_dropped++;
            return;
        }
    }
}

/* Copia `len` bytes para o ring (pode fazer wrap em até dois memcpy). */
static void ring_write(const char *data, uint32_t len)
{
    uint32_t to_end = CAP - s_head;
    if (len <= to_end) {
        memcpy(s_buf + s_head, data, len);
    } else {
        memcpy(s_buf + s_head, data, to_end);
        memcpy(s_buf,           data + to_end, len - to_end);
    }
    s_head = (s_head + len) % CAP;
    s_stat_written++;
}

/* ══════════════════════════════════════════════════════════════════════
   Rotação de arquivo
   ══════════════════════════════════════════════════════════════════════ */
static void check_rotation(void)
{
    struct stat st;
    if (stat(LOG_SD_PATH "/log_0.jsonl", &st) != 0) return;
    if ((uint32_t)st.st_size < LOG_MAX_FILE_SIZE) return;

    /* Remove log_2 → desloca log_1 → desloca log_0 */
    remove (LOG_SD_PATH "/log_2.jsonl");
    rename (LOG_SD_PATH "/log_1.jsonl", LOG_SD_PATH "/log_2.jsonl");
    rename (LOG_SD_PATH "/log_0.jsonl", LOG_SD_PATH "/log_1.jsonl");

    ESP_LOGI(TAG, "rotação: log_0 → log_1  (log_2 removido)");
}

/* ══════════════════════════════════════════════════════════════════════
   Flush interno (chamado com s_flush_mtx adquirido)
   ══════════════════════════════════════════════════════════════════════ */
static void do_flush(void)
{
    /* Snapshot atômico de head e tail */
    taskENTER_CRITICAL(&s_mux);
    uint32_t snap_head = s_head;
    uint32_t snap_tail = s_tail;
    taskEXIT_CRITICAL(&s_mux);

    if (snap_head == snap_tail) return;   /* buffer vazio */

    FILE *f = fopen(LOG_SD_PATH "/log_0.jsonl", "a");
    if (!f) {
        ESP_LOGE(TAG, "fopen log_0.jsonl falhou");
        return;
    }

    uint32_t bytes = 0;
    if (snap_head > snap_tail) {
        /* Dados contíguos */
        bytes = (uint32_t)fwrite(s_buf + snap_tail, 1,
                                  snap_head - snap_tail, f);
    } else {
        /* Dados em dois segmentos (wrap) */
        bytes  = (uint32_t)fwrite(s_buf + snap_tail, 1, CAP - snap_tail, f);
        bytes += (uint32_t)fwrite(s_buf, 1, snap_head, f);
    }

    fclose(f);

    /* Avança tail até o snapshot de head */
    taskENTER_CRITICAL(&s_mux);
    s_tail = snap_head;
    s_stat_flushed += bytes;
    taskEXIT_CRITICAL(&s_mux);

    check_rotation();
}

/* ══════════════════════════════════════════════════════════════════════
   Task de flush periódico
   ══════════════════════════════════════════════════════════════════════ */
static void flush_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* Bloqueia por LOG_FLUSH_PERIOD_MS ou até receber notificação */
        ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(LOG_FLUSH_PERIOD_MS));
        log_flush_now();
    }
}

/* ══════════════════════════════════════════════════════════════════════
   API pública
   ══════════════════════════════════════════════════════════════════════ */

esp_err_t log_init(void)
{
    /* Aloca em PSRAM; fallback para SRAM se não disponível */
    s_buf = heap_caps_malloc(CAP, MALLOC_CAP_SPIRAM);
    if (!s_buf) {
        ESP_LOGW(TAG, "PSRAM indisponível — usando SRAM");
        s_buf = malloc(CAP);
    }
    if (!s_buf) return ESP_ERR_NO_MEM;

    s_head = s_tail = 0;
    s_stat_written = s_stat_dropped = s_stat_flushed = 0;

    s_flush_mtx = xSemaphoreCreateMutex();
    if (!s_flush_mtx) { free(s_buf); return ESP_ERR_NO_MEM; }

    BaseType_t r = xTaskCreate(flush_task, "log_flush", 4096, NULL, 5, &s_flush_task);
    if (r != pdPASS) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "init OK  buf=%u B  PSRAM=%s  rot_at=%u B",
             (unsigned)CAP,
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0 ? "sim" : "nao",
             (unsigned)LOG_MAX_FILE_SIZE);
    return ESP_OK;
}

esp_err_t log_write(log_level_t level, const char *service, const char *message)
{
    if (!s_buf) return ESP_ERR_INVALID_STATE;
    if ((unsigned)level >= LOG_LEVEL_MAX) level = LOG_DEBUG;

    /* ── Formata JSON na stack — FORA do spinlock ─────────────────── */
    char line[LOG_MAX_LINE];
    uint32_t ts_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    int len = snprintf(line, sizeof(line),
                       "{\"l\":\"%s\",\"s\":\"%s\",\"m\":\"%s\",\"t\":%" PRIu32 "}\n",
                       s_lvl[level], service, message, ts_ms);
    if (len <= 0) return ESP_FAIL;
    if ((uint32_t)len >= sizeof(line)) len = (int)sizeof(line) - 1;

    /* ── Copia para ring — spinlock curto (apenas memcpy + scan) ──── */
    taskENTER_CRITICAL(&s_mux);
    ring_make_room((uint32_t)len);
    ring_write(line, (uint32_t)len);
    taskEXIT_CRITICAL(&s_mux);

    return ESP_OK;
}

void log_flush_now(void)
{
    if (!s_buf || !s_flush_mtx) return;
    /* xTaskNotifyGive acorda a flush_task se chamado de outra task */
    if (s_flush_task && xTaskGetCurrentTaskHandle() != s_flush_task)
        xTaskNotifyGive(s_flush_task);

    if (xSemaphoreTake(s_flush_mtx, pdMS_TO_TICKS(5000)) != pdTRUE) return;
    do_flush();
    xSemaphoreGive(s_flush_mtx);
}

void log_get_stats(uint32_t *written, uint32_t *dropped, uint32_t *flushed_bytes)
{
    taskENTER_CRITICAL(&s_mux);
    if (written)       *written       = s_stat_written;
    if (dropped)       *dropped       = s_stat_dropped;
    if (flushed_bytes) *flushed_bytes = s_stat_flushed;
    taskEXIT_CRITICAL(&s_mux);
}
