#include "kws_recorder.h"
#include "audio_capture.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_log_level.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static const char *TAG = "kws_rec";

/* ── Parâmetros ────────────────────────────────────────────────────────── */
#define REC_SAMPLE_RATE   16000u
#define REC_DURATION_MS    2000u   /* 2.0s por template (trim remove silêncio depois) */
#define REC_SAMPLES        (REC_SAMPLE_RATE * REC_DURATION_MS / 1000u)  /* 24000 */
#define KWS_DIR            "/sdcard/kws"
#define N_KEYWORDS         12
#define N_TPL_PER_KW        5

static const char * const s_kw_names[N_KEYWORDS] = {
    "dorme",        /* 0  */
    "acorda",       /* 1  */
    "silencio",     /* 2  */
    "privado",      /* 3  */
    "que_horas",    /* 4  */
    "como_voce",    /* 5  */
    "olhe_para_mim",/* 6  */
    "vol_alto",     /* 7  */
    "vol_baixo",    /* 8  */
    "sim",          /* 9  */
    "nao",          /* 10 */
    "cancela",      /* 11 */
};

/* ── WAV writer ─────────────────────────────────────────────────────────── */
static void write_u16(FILE *f, uint16_t v)
{
    fwrite(&v, 2, 1, f);
}

static void write_u32(FILE *f, uint32_t v)
{
    fwrite(&v, 4, 1, f);
}

static esp_err_t save_wav(const char *path, const int16_t *pcm, size_t samples)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "fopen falhou: %s", path);
        return ESP_FAIL;
    }

    uint32_t data_bytes = (uint32_t)(samples * 2u);
    uint32_t file_size  = 36u + data_bytes;

    /* RIFF header */
    fwrite("RIFF", 1, 4, f);
    write_u32(f, file_size);
    fwrite("WAVE", 1, 4, f);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, f);
    write_u32(f, 16u);                  /* chunk size   */
    write_u16(f, 1u);                   /* PCM          */
    write_u16(f, 1u);                   /* mono         */
    write_u32(f, REC_SAMPLE_RATE);      /* 16000 Hz     */
    write_u32(f, REC_SAMPLE_RATE * 2u); /* byte rate    */
    write_u16(f, 2u);                   /* block align  */
    write_u16(f, 16u);                  /* bits/sample  */

    /* data chunk */
    fwrite("data", 1, 4, f);
    write_u32(f, data_bytes);
    fwrite(pcm, 2, samples, f);

    fclose(f);
    return ESP_OK;
}

/* ── VAD trim ────────────────────────────────────────────────────────────── */
/*
 * Encontra os limites de fala em um buffer de áudio.
 * Analisa blocos de 10ms; considera "fala" quando energia > TRIM_THR_SQ.
 * Retorna start/end com margens de 50ms para não cortar onset/offset.
 */
#define TRIM_BLOCK       160u    /* 10ms @ 16kHz                     */
#define TRIM_THR_SQ      (150u * 150u)  /* RMS² mínimo (~-47 dBFS)  */
#define TRIM_MARGIN      800u    /* 50ms de margem nas bordas        */
#define TRIM_MIN_SPEECH  3u      /* mín 3 blocos consecutivos = 30ms */

static void trim_speech(const int16_t *src, size_t n,
                         size_t *out_start, size_t *out_end)
{
    *out_start = 0;
    *out_end   = n;

    /* Procura início de fala (da esquerda) */
    uint32_t run = 0;
    for (size_t off = 0; off + TRIM_BLOCK <= n; off += TRIM_BLOCK) {
        int64_t sq = 0;
        for (size_t i = 0; i < TRIM_BLOCK; i++) {
            int32_t v = src[off + i]; sq += v * v;
        }
        uint32_t e = (uint32_t)(sq / TRIM_BLOCK);
        if (e >= TRIM_THR_SQ) {
            if (++run >= TRIM_MIN_SPEECH) {
                size_t start = off - (run - 1u) * TRIM_BLOCK;
                *out_start = (start >= TRIM_MARGIN) ? (start - TRIM_MARGIN) : 0;
                break;
            }
        } else {
            run = 0;
        }
    }

    /* Procura fim de fala (da direita) */
    run = 0;
    for (size_t off = n; off >= TRIM_BLOCK; off -= TRIM_BLOCK) {
        int64_t sq = 0;
        for (size_t i = 0; i < TRIM_BLOCK; i++) {
            int32_t v = src[off - TRIM_BLOCK + i]; sq += v * v;
        }
        uint32_t e = (uint32_t)(sq / TRIM_BLOCK);
        if (e >= TRIM_THR_SQ) {
            if (++run >= TRIM_MIN_SPEECH) {
                size_t end = off + (run - 1u) * TRIM_BLOCK;
                if (end > n) end = n;
                *out_end = (end + TRIM_MARGIN <= n) ? (end + TRIM_MARGIN) : n;
                break;
            }
        } else {
            run = 0;
        }
    }

    /* Garante que start < end com mínimo de 160ms */
    if (*out_start >= *out_end || (*out_end - *out_start) < 2560u)
        { *out_start = 0; *out_end = n; }  /* fallback: sem trim */
}

/* ── stdin helper ───────────────────────────────────────────────────────── */

/* Descarta qualquer lixo acumulado no stdin (resíduos do boot). */
static void flush_stdin(void)
{
    /* Lê e descarta até não haver mais dados disponíveis.
     * fgetc com stdin configurado como não-bloqueante retorna EOF quando vazio. */
    int c;
    /* Dá 200 ms para o buffer estabilizar antes de limpar */
    vTaskDelay(pdMS_TO_TICKS(200));
    while ((c = fgetc(stdin)) != EOF && c != '\0') {
        /* descarta */
    }
}

/* Aguarda ENTER ou 'q'. Retorna o caractere relevante ou -1 em timeout. */
static int wait_for_enter(void)
{
    int c;
    while (1) {
        c = fgetc(stdin);
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (c == '\r' || c == '\n' || c == 'q' || c == 'Q' || c == 'c' || c == 'C') return c;
    }
}

/* ── Captura via PCM listener (independe do ring buffer) ────────────────── */
static int16_t          *s_rec_buf   = NULL;
static volatile size_t   s_rec_idx   = 0;
static volatile size_t   s_rec_total = 0;
static SemaphoreHandle_t s_rec_sem   = NULL;

static void rec_pcm_listener(const int16_t *pcm, size_t len)
{
    size_t space = s_rec_total - s_rec_idx;
    size_t copy  = len < space ? len : space;
    if (copy > 0) {
        memcpy(s_rec_buf + s_rec_idx, pcm, copy * sizeof(int16_t));
        s_rec_idx += copy;
    }
    if (s_rec_idx >= s_rec_total) {
        audio_capture_set_pcm_listener(NULL);
        xSemaphoreGiveFromISR(s_rec_sem, NULL);
    }
}

/* ── Gravação de um template ─────────────────────────────────────────────── */
static esp_err_t record_template(int16_t *buf, const char *path)
{
    /* Countdown */
    for (int i = 3; i >= 1; i--) {
        printf("%d... ", i);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(700));
    }
    printf("FALE!\n");
    fflush(stdout);

    /* Arma listener */
    s_rec_buf   = buf;
    s_rec_idx   = 0;
    s_rec_total = REC_SAMPLES;
    audio_capture_set_pcm_listener(rec_pcm_listener);

    /* Aguarda conclusão (timeout = duração + 1s de margem) */
    TickType_t timeout = pdMS_TO_TICKS(REC_DURATION_MS + 1000);
    if (xSemaphoreTake(s_rec_sem, timeout) != pdTRUE) {
        audio_capture_set_pcm_listener(NULL);
        printf("  TIMEOUT — sem áudio do microfone\n");
        fflush(stdout);
        return ESP_FAIL;
    }

    /* Detecta limites de fala e salva apenas o trecho com voz */
    size_t t_start = 0, t_end = REC_SAMPLES;
    trim_speech(buf, REC_SAMPLES, &t_start, &t_end);
    size_t t_len = t_end - t_start;

    printf("  gravado %ums → trim [%ums..%ums] (%ums) — salvando %s ...",
           (unsigned)(REC_DURATION_MS),
           (unsigned)(t_start * 1000u / REC_SAMPLE_RATE),
           (unsigned)(t_end   * 1000u / REC_SAMPLE_RATE),
           (unsigned)(t_len   * 1000u / REC_SAMPLE_RATE),
           path);
    fflush(stdout);

    esp_err_t err = save_wav(path, buf + t_start, t_len);
    printf(" %s\n", err == ESP_OK ? "OK" : "ERRO");
    fflush(stdout);
    return err;
}

/* ── API pública ─────────────────────────────────────────────────────────── */
esp_err_t kws_recorder_run(void)
{
    /* Buffer para uma gravação — aloca na heap normal (24000 × 2 = 48KB) */
    int16_t *buf = malloc(REC_SAMPLES * sizeof(int16_t));
    if (!buf) {
        ESP_LOGE(TAG, "sem memória para buf de gravação (%u bytes)",
                 (unsigned)(REC_SAMPLES * sizeof(int16_t)));
        return ESP_ERR_NO_MEM;
    }

    /* Semáforo de sincronização com o PCM listener */
    s_rec_sem = xSemaphoreCreateBinary();
    if (!s_rec_sem) {
        free(buf);
        return ESP_ERR_NO_MEM;
    }

    /* Silencia todos os logs enquanto o recorder estiver ativo */
    esp_log_level_set("*", ESP_LOG_NONE);

    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║        KWS RECORDER — NodeBot Templates          ║\n");
    printf("║  %d keywords × %d gravações = %d templates        ║\n",
           N_KEYWORDS, N_TPL_PER_KW, N_KEYWORDS * N_TPL_PER_KW);
    printf("║  Duração por gravação: %d ms                     ║\n", REC_DURATION_MS);
    printf("║  Pasta: %-40s║\n", KWS_DIR);
    printf("║  ENTER = gravar  |  'q' = sair  |  'c' = limpar ║\n");
    printf("╚══════════════════════════════════════════════════╝\n\n");
    fflush(stdout);

    /* ── Opção de limpeza ──────────────────────────────────────────────── */
    printf("Pressione 'c' para apagar todos os templates existentes,\n");
    printf("ou qualquer outra tecla para continuar/retomar: ");
    fflush(stdout);
    flush_stdin();
    int first = wait_for_enter();
    if (first == 'c' || first == 'C') {
        printf("\nApagando templates...\n");
        char path[128];
        int deleted = 0;
        for (int kw = 0; kw < N_KEYWORDS; kw++) {
            for (int idx = 0; idx < N_TPL_PER_KW; idx++) {
                snprintf(path, sizeof(path), "%s/%s_%d.wav",
                         KWS_DIR, s_kw_names[kw], idx);
                if (remove(path) == 0) deleted++;
            }
        }
        printf("%d arquivos removidos. Iniciando gravação do zero.\n\n", deleted);
        fflush(stdout);
    } else {
        printf("\nRetomando — arquivos existentes serão pulados.\n\n");
        fflush(stdout);
    }

    int errors   = 0;
    int recorded = 0;

    for (int kw = 0; kw < N_KEYWORDS; kw++) {
        printf("━━━ Keyword %d/%d: \"%s\" ━━━\n",
               kw + 1, N_KEYWORDS, s_kw_names[kw]);
        fflush(stdout);

        for (int idx = 0; idx < N_TPL_PER_KW; idx++) {
            printf("  [%d/%d] Pressione ENTER para gravar \"%s\" (ou 'q' para sair): ",
                   idx + 1, N_TPL_PER_KW, s_kw_names[kw]);
            fflush(stdout);

            /* Aguarda ENTER ou 'q' */
            flush_stdin();
            int c = wait_for_enter();

            if (c == 'q' || c == 'Q') {
                printf("\nAbortado pelo usuário. %d templates gravados.\n", recorded);
                fflush(stdout);
                esp_log_level_set("*", ESP_LOG_INFO);
                vSemaphoreDelete(s_rec_sem);
                s_rec_sem = NULL;
                free(buf);
                return ESP_ERR_INVALID_STATE;
            }

            char path[128];
            snprintf(path, sizeof(path), "%s/%s_%d.wav",
                     KWS_DIR, s_kw_names[kw], idx);

            /* Pula se já existe — permite retomar após interrupção */
            FILE *chk = fopen(path, "rb");
            if (chk) {
                fclose(chk);
                printf("  já existe, pulando.\n");
                fflush(stdout);
                recorded++;
                continue;
            }

            esp_err_t err = record_template(buf, path);
            if (err == ESP_OK) {
                recorded++;
            } else {
                errors++;
            }

            /* Pausa entre gravações para o usuário se preparar */
            vTaskDelay(pdMS_TO_TICKS(800));
        }
        printf("\n");
    }

    free(buf);

    printf("════════════════════════════════════════════\n");
    printf("Concluído: %d gravados, %d erros.\n", recorded, errors);
    printf("Reinicie o dispositivo para carregar os novos templates.\n");
    printf("════════════════════════════════════════════\n\n");
    fflush(stdout);

    /* Restaura logs normais e libera recursos */
    esp_log_level_set("*", ESP_LOG_INFO);
    vSemaphoreDelete(s_rec_sem);
    s_rec_sem = NULL;

    return (errors == 0) ? ESP_OK : ESP_FAIL;
}

/* ── Console task ────────────────────────────────────────────────────────── */

static void console_task(void *arg)
{
    (void)arg;
    printf("[kws_rec] console ativo — digite 'R' para gravar templates\n");
    fflush(stdout);

    while (1) {
        int c = fgetc(stdin);
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (c == 'r' || c == 'R') {
            printf("\n[kws_rec] iniciando recorder...\n");
            fflush(stdout);
            kws_recorder_run();
            printf("[kws_rec] recorder encerrado — recarregue templates com reset\n");
            fflush(stdout);
        }
    }
}

void kws_recorder_console_start(void)
{
    xTaskCreatePinnedToCore(console_task, "kws_console",
                            3072, NULL, 2, NULL, 1 /* Core 1 */);
}
