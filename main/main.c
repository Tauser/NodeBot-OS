#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>

#include "log_manager.h"
#include "sd_driver.h"

static const char *TAG = "test_log";
static uint32_t s_pass, s_fail;

#define ASSERT(cond, msg)                                                     \
    do {                                                                       \
        if (cond) { ESP_LOGI(TAG, "  PASS  %s", msg); s_pass++; }            \
        else { ESP_LOGE(TAG, "  FAIL  %s  (%s:%d)", msg, __FILE__, __LINE__);\
               s_fail++; }                                                     \
    } while (0)

#define SKIP(msg) ESP_LOGI(TAG, "  SKIP  %s  (SD indisponível)", msg)

/* ════════════════════════════════════════════════════════════════════════ */

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "  LogManager — teste E14");
    ESP_LOGI(TAG, "════════════════════════════════════════");

    /* ── SD init (opcional) ──────────────────────────────────────────── */
    bool sd_ok = (sd_init() == ESP_OK);
    if (sd_ok)  ESP_LOGI(TAG, "SD montado em /sdcard");
    else        ESP_LOGW(TAG, "SD indisponível — testes de arquivo pulados");

    /* ── T1: init ────────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "T1: init");
    ASSERT(log_init() == ESP_OK, "log_init retorna ESP_OK");

    /* ── T2: write em todos os níveis ────────────────────────────────── */
    ESP_LOGI(TAG, "T2: write — todos os níveis");
    ASSERT(log_write(LOG_FATAL, "sys",   "fatal test entry") == ESP_OK, "write FATAL");
    ASSERT(log_write(LOG_ERROR, "imu",   "error test entry") == ESP_OK, "write ERROR");
    ASSERT(log_write(LOG_WARN,  "audio", "warn  test entry") == ESP_OK, "write WARN");
    ASSERT(log_write(LOG_INFO,  "led",   "info  test entry") == ESP_OK, "write INFO");
    ASSERT(log_write(LOG_DEBUG, "touch", "debug test entry") == ESP_OK, "write DEBUG");

    /* ── T3: latência — NÃO BLOQUEANTE (<100 µs) ─────────────────────── */
    ESP_LOGI(TAG, "T3: latencia (1000 amostras)");

    uint32_t min_us = UINT32_MAX, max_us = 0, sum_us = 0;
    /* Descarta primeiros 10 (cache/pipeline warm-up) */
    for (int i = 0; i < 10; i++)
        log_write(LOG_DEBUG, "warm", "warmup");

    for (int i = 0; i < 1000; i++) {
        uint64_t t0 = esp_timer_get_time();
        log_write(LOG_INFO, "perf", "latency sample 0123456789abcdef");
        uint64_t t1 = esp_timer_get_time();
        uint32_t us = (uint32_t)(t1 - t0);
        if (us < min_us) min_us = us;
        if (us > max_us) max_us = us;
        sum_us += us;
    }
    uint32_t avg_us = sum_us / 1000u;

    ESP_LOGI(TAG, "  min=%" PRIu32 " µs  avg=%" PRIu32 " µs  max=%" PRIu32 " µs",
             min_us, avg_us, max_us);
    ASSERT(avg_us < 100u, "latência média < 100 µs");
    ASSERT(max_us < 500u, "latência máxima < 500 µs");

    /* ── T4: flush para SD ───────────────────────────────────────────── */
    ESP_LOGI(TAG, "T4: flush");
    if (sd_ok) {
        log_flush_now();
        ASSERT(true, "log_flush_now() concluiu sem travar");
    } else { SKIP("log_flush_now"); }

    /* ── T5: arquivo log_0.jsonl existe e não está vazio ─────────────── */
    ESP_LOGI(TAG, "T5: log_0.jsonl no SD");
    long size_after_t4 = 0;
    if (sd_ok) {
        struct stat st;
        int r = stat(LOG_SD_PATH "/log_0.jsonl", &st);
        ASSERT(r == 0, "log_0.jsonl existe");
        if (r == 0) {
            size_after_t4 = st.st_size;
            ASSERT(st.st_size > 0, "log_0.jsonl tamanho > 0");
            ESP_LOGI(TAG, "  log_0.jsonl = %ld B", st.st_size);

            /* Lê a primeira linha e verifica o prefixo JSON */
            FILE *f = fopen(LOG_SD_PATH "/log_0.jsonl", "r");
            if (f) {
                char buf[LOG_MAX_LINE];
                char *line = fgets(buf, sizeof(buf), f);
                fclose(f);
                ASSERT(line && buf[0] == '{', "primeira linha é JSON (começa com '{')");
            }
        }
    } else { SKIP("verificação log_0.jsonl"); }

    /* ── T6: overflow de buffer → drop handling ──────────────────────── */
    /*
     * Buffer = 16 KB; entrada ~68 B ("latency sample..." + JSON overhead).
     * ~240 entradas cabem → escrever 300 sem flush deve gerar drops.
     */
    ESP_LOGI(TAG, "T6: overflow de buffer / drop");
    uint32_t w0, d0;
    log_get_stats(&w0, &d0, NULL);

    for (int i = 0; i < 300; i++)
        log_write(LOG_DEBUG, "ovf", "overflow test filling the 16K ring buffer 0123456789");

    uint32_t w1, d1;
    log_get_stats(&w1, &d1, NULL);

    ESP_LOGI(TAG, "  +written=%" PRIu32 "  +dropped=%" PRIu32, w1-w0, d1-d0);
    ASSERT(w1 - w0 == 300u, "300 chamadas log_write contabilizadas");
    ASSERT(d1 > d0,         "drop detectado quando buffer encheu");

    /* ── T7: flush final drena overflow ──────────────────────────────── */
    ESP_LOGI(TAG, "T7: flush final (drena overflow)");
    if (sd_ok) {
        log_flush_now();
        struct stat st2;
        stat(LOG_SD_PATH "/log_0.jsonl", &st2);
        ESP_LOGI(TAG, "  log_0.jsonl = %ld B  (+%ld B vs T4)",
                 st2.st_size, st2.st_size - size_after_t4);
        ASSERT(st2.st_size > size_after_t4, "arquivo cresceu após flush do overflow");
    } else { SKIP("flush final"); }

    /* ── T8: stats globais ───────────────────────────────────────────── */
    ESP_LOGI(TAG, "T8: stats globais");
    uint32_t pub, drp, flb;
    log_get_stats(&pub, &drp, &flb);
    ESP_LOGI(TAG, "  written=%" PRIu32 "  dropped=%" PRIu32 "  flushed=%" PRIu32 " B",
             pub, drp, flb);
    ASSERT(pub  > 0, "written > 0");
    ASSERT(drp  > 0, "dropped > 0  (T6 overflow confirmado)");
    if (sd_ok)
        ASSERT(flb > 0, "flushed_bytes > 0");

    /* ── Resultado ───────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "  Resultado: PASS=%" PRIu32 "  FAIL=%" PRIu32, s_pass, s_fail);
    if (s_fail == 0)
        ESP_LOGI(TAG, "  ✓ todos os testes passaram");
    else
        ESP_LOGE(TAG, "  ✗ %" PRIu32 " teste(s) falharam", s_fail);
    ESP_LOGI(TAG, "════════════════════════════════════════");
}
