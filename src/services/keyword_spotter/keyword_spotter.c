#include "keyword_spotter.h"

#include "dsps_fft2r.h"
#include "dsp_err.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static const char *TAG = "kws";

/* ── Parâmetros MFCC ───────────────────────────────────────────────────── */
#define KWS_SAMPLE_RATE        16000
#define KWS_FRAME_SAMPLES        400   /* 25 ms                              */
#define KWS_HOP_SAMPLES          160   /* 10 ms                              */
#define KWS_FFT_SIZE             512   /* zero-pad 400 → 512                 */
#define KWS_N_MEL                 26   /* filtros mel                        */
#define KWS_N_MFCC                13   /* coeficientes cepstrais             */
#define KWS_MEL_FMIN             80.0f
#define KWS_MEL_FMAX           8000.0f
#define KWS_MAX_FRAMES           100   /* 1.0s max por template              */
#define KWS_MAX_QUERY_FRAMES     300   /* 3.0s max da query                  */
#define KWS_N_TPL_PER_KW           5
#define KWS_MAX_TEMPLATES        (KWS_KEYWORDS * KWS_N_TPL_PER_KW)  /* 60  */
#define KWS_DTW_BAND              20   /* largura da banda Sakoe-Chiba       */
#define KWS_MATCH_THRESHOLD    150.0f  /* dist/M observado: acorda=122, vol_baixo=76 com query 281fr;
                                        * com query curta (~50fr) espera-se 20-60 (match) */
#define KWS_MIN_MATCH_FRAMES       5   /* query muito curta → no match       */

/* ── Nomes das keywords (índice = keyword_id) ──────────────────────────── */
static const char * const s_kw_names[KWS_KEYWORDS] = {
    "dorme",      /* 0  */
    "acorda",     /* 1  */
    "silencio",   /* 2  */
    "privado",    /* 3  */
    "que_horas",  /* 4  */
    "como_voce",  /* 5  */
    "olhe_para_mim",    /* 6  */
    "vol_alto",   /* 7  */
    "vol_baixo",  /* 8  */
    "sim",        /* 9  */
    "nao",        /* 10 */
    "cancela",    /* 11 */
};

/* ── Estrutura de template ─────────────────────────────────────────────── */
typedef struct {
    float    mfcc[KWS_MAX_FRAMES][KWS_N_MFCC];
    uint16_t n_frames;
    uint8_t  keyword_id;
    bool     valid;
} kws_template_t;

/* ── Estado global ─────────────────────────────────────────────────────── */
static kws_template_t *s_templates   = NULL;   /* KWS_MAX_TEMPLATES em PSRAM */
static float          *s_query_mfcc  = NULL;   /* MAX_QUERY_FRAMES×N_MFCC    */
static float          *s_fft_buf     = NULL;   /* FFT_SIZE×2 complex floats  */
static int             s_n_templates = 0;

/* Tabelas pré-computadas (SRAM — pequenas) */
static float    s_hann[KWS_FRAME_SAMPLES];
static uint16_t s_mel_bin[KWS_N_MEL + 2];          /* bins dos 26+2 pontos  */
static float    s_dct_matrix[KWS_N_MFCC][KWS_N_MEL];

/* Buffers de trabalho DTW (SRAM) */
static float s_dtw_row0[KWS_MAX_FRAMES + 1];
static float s_dtw_row1[KWS_MAX_FRAMES + 1];

/* ── Mel helpers ───────────────────────────────────────────────────────── */
static inline float hz_to_mel(float hz)
{
    return 2595.0f * log10f(1.0f + hz / 700.0f);
}

static inline float mel_to_hz(float mel)
{
    return 700.0f * (powf(10.0f, mel / 2595.0f) - 1.0f);
}

/* ── Inicialização de tabelas ──────────────────────────────────────────── */
static void init_hann(void)
{
    for (int i = 0; i < KWS_FRAME_SAMPLES; i++) {
        s_hann[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i
                                         / (float)(KWS_FRAME_SAMPLES - 1)));
    }
}

static void init_mel_bins(void)
{
    float mel_min  = hz_to_mel(KWS_MEL_FMIN);
    float mel_max  = hz_to_mel(KWS_MEL_FMAX);
    float mel_step = (mel_max - mel_min) / (float)(KWS_N_MEL + 1);
    int   n_bins   = KWS_FFT_SIZE / 2;

    for (int i = 0; i < KWS_N_MEL + 2; i++) {
        float mel  = mel_min + (float)i * mel_step;
        float hz   = mel_to_hz(mel);
        float binf = hz * (float)KWS_FFT_SIZE / (float)KWS_SAMPLE_RATE;
        int   bin  = (int)(binf + 0.5f);
        if (bin < 1)       bin = 1;
        if (bin >= n_bins) bin = n_bins - 1;
        s_mel_bin[i] = (uint16_t)bin;
    }
}

static void init_dct_matrix(void)
{
    for (int n = 0; n < KWS_N_MFCC; n++) {
        for (int m = 0; m < KWS_N_MEL; m++) {
            s_dct_matrix[n][m] = cosf((float)M_PI * (float)n
                                       * ((float)m + 0.5f) / (float)KWS_N_MEL);
        }
    }
}

/* ── Extração de MFCC para um único frame ──────────────────────────────── */
static void compute_frame_mfcc(const int16_t *frame, float *mfcc_out)
{
    /* Pré-ênfase + Hanning → buffer complexo interleaved [Re0,Im0,...] */
    float prev = 0.0f;
    for (int i = 0; i < KWS_FRAME_SAMPLES; i++) {
        float y = (float)frame[i] - 0.97f * prev;
        prev = (float)frame[i];
        y *= s_hann[i];
        s_fft_buf[2 * i]     = y;
        s_fft_buf[2 * i + 1] = 0.0f;
    }
    /* Zero-pad até FFT_SIZE */
    for (int i = KWS_FRAME_SAMPLES; i < KWS_FFT_SIZE; i++) {
        s_fft_buf[2 * i]     = 0.0f;
        s_fft_buf[2 * i + 1] = 0.0f;
    }

    /* FFT + bit-reversal */
    dsps_fft2r_fc32_ansi(s_fft_buf, KWS_FFT_SIZE);
    dsps_bit_rev_fc32_ansi(s_fft_buf, KWS_FFT_SIZE);

    /* Espectro de potência P[k] = Re²+Im²  (k=0..N/2-1) */
    int n_bins = KWS_FFT_SIZE / 2;
    for (int k = 0; k < n_bins; k++) {
        float re = s_fft_buf[2 * k];
        float im = s_fft_buf[2 * k + 1];
        s_fft_buf[k] = re * re + im * im;   /* reusa buffer (k < 2k) */
    }

    /* Mel filterbank → log-energias */
    float log_fb[KWS_N_MEL];
    for (int m = 0; m < KWS_N_MEL; m++) {
        int bl = s_mel_bin[m];
        int bc = s_mel_bin[m + 1];
        int br = s_mel_bin[m + 2];
        float energy = 0.0f;

        if (bc > bl) {
            float w_up = 1.0f / (float)(bc - bl);
            for (int k = bl; k < bc && k < n_bins; k++) {
                energy += s_fft_buf[k] * (float)(k - bl) * w_up;
            }
        }
        if (br > bc) {
            float w_dn = 1.0f / (float)(br - bc);
            for (int k = bc; k < br && k < n_bins; k++) {
                energy += s_fft_buf[k] * (float)(br - k) * w_dn;
            }
        }
        log_fb[m] = logf(energy + 1e-10f);
    }

    /* DCT-II → 13 MFCCs */
    for (int n = 0; n < KWS_N_MFCC; n++) {
        float sum = 0.0f;
        for (int m = 0; m < KWS_N_MEL; m++) {
            sum += log_fb[m] * s_dct_matrix[n][m];
        }
        mfcc_out[n] = sum;
    }
}

/* ── Extração de MFCC para um buffer de áudio completo ─────────────────── */
/* Retorna número de frames extraídos. Aplica CMN ao final. */
static int extract_mfcc(const int16_t *audio, size_t samples,
                         float *mfcc_out, int max_frames)
{
    int    n_frames = 0;
    size_t offset   = 0;

    while ((offset + KWS_FRAME_SAMPLES) <= samples && n_frames < max_frames) {
        compute_frame_mfcc(audio + offset,
                            mfcc_out + (size_t)n_frames * KWS_N_MFCC);
        n_frames++;
        offset += KWS_HOP_SAMPLES;
    }

    if (n_frames == 0) return 0;

    /* CMN: subtrai a média de cada coeficiente ao longo dos frames */
    for (int c = 0; c < KWS_N_MFCC; c++) {
        float mean = 0.0f;
        for (int f = 0; f < n_frames; f++) {
            mean += mfcc_out[(size_t)f * KWS_N_MFCC + c];
        }
        mean /= (float)n_frames;
        for (int f = 0; f < n_frames; f++) {
            mfcc_out[(size_t)f * KWS_N_MFCC + c] -= mean;
        }
    }
    return n_frames;
}

/* ── Subsequence DTW (2 linhas, sem malloc) ─────────────────────────────── */
/* Encontra o melhor alinhamento do template T em qualquer janela de Q.     */
/* Sem banda Sakoe-Chiba — necessário quando len(Q) >> len(T).              */
/* Normaliza por M (tamanho do template) para comparações cross-keyword.    */
static float dtw_distance(const float *Q, int N, const float *T, int M)
{
    float *prev = s_dtw_row0;
    float *curr = s_dtw_row1;

    /* Inicializa prev com infinito — nenhum caminho ainda */
    for (int j = 0; j <= M; j++) prev[j] = FLT_MAX;

    float min_dist = FLT_MAX;

    for (int i = 0; i < N; i++) {
        /* curr[0] = 0: permite iniciar um novo alinhamento neste frame de Q */
        curr[0] = 0.0f;

        for (int j = 0; j < M; j++) {
            float d = 0.0f;
            const float *qi = Q + (size_t)i * KWS_N_MFCC;
            const float *tj = T + (size_t)j * KWS_N_MFCC;
            /* k=0 é C0 (energia absoluta) — sensível a ganho e microfone, omitido */
            for (int k = 1; k < KWS_N_MFCC; k++) {
                float diff = qi[k] - tj[k];
                d += diff * diff;
            }

            float a  = prev[j];        /* diagonal (i-1, j-1) */
            float b  = prev[j + 1];    /* cima     (i-1, j)   */
            float c  = curr[j];        /* esquerda (i,   j-1) */
            float mn = a < b ? a : b;
            if (c < mn) mn = c;

            curr[j + 1] = (mn >= FLT_MAX / 2.0f) ? FLT_MAX : (d + mn);
        }

        /* Registra o melhor match completo do template terminando aqui */
        if (curr[M] < min_dist) min_dist = curr[M];

        float *tmp = prev; prev = curr; curr = tmp;
    }

    return (min_dist >= FLT_MAX / 2.0f) ? 1e10f : min_dist / (float)M;
}

/* ── Carregamento de arquivo WAV ───────────────────────────────────────── */
/* Aceita WAV PCM 16-bit mono 16kHz. Lida com chunks extras antes de "data". */
static size_t load_wav(const char *path, int16_t *buf, size_t max_samples)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    char     id[4];
    uint32_t sz;

    /* RIFF header */
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4) != 0) goto fail;
    if (fread(&sz, 4, 1, f)  != 1)                               goto fail;
    if (fread(id, 1, 4, f)  != 4 || memcmp(id, "WAVE", 4) != 0) goto fail;

    bool fmt_ok = false;

    while (fread(id, 1, 4, f) == 4) {
        if (fread(&sz, 4, 1, f) != 1) break;

        if (memcmp(id, "fmt ", 4) == 0) {
            if (sz < 16) goto fail;
            uint16_t fmt, ch, bits;
            uint32_t sr;
            if (fread(&fmt,  2, 1, f) != 1) goto fail;
            if (fread(&ch,   2, 1, f) != 1) goto fail;
            if (fread(&sr,   4, 1, f) != 1) goto fail;
            fseek(f, 6, SEEK_CUR);   /* byte_rate + block_align */
            if (fread(&bits, 2, 1, f) != 1) goto fail;
            fseek(f, (long)sz - 16 + (sz & 1u), SEEK_CUR);
            fmt_ok = (fmt == 1 && ch == 1 && sr == 16000u && bits == 16);

        } else if (memcmp(id, "data", 4) == 0) {
            if (!fmt_ok) goto fail;
            size_t n = sz / 2;
            if (n > max_samples) n = max_samples;
            n = fread(buf, 2, n, f);
            fclose(f);
            return n;

        } else {
            fseek(f, (long)sz + (long)(sz & 1u), SEEK_CUR);
        }
    }

fail:
    fclose(f);
    return 0;
}

/* ── API pública ───────────────────────────────────────────────────────── */

const char *keyword_spotter_name(int keyword_id)
{
    if (keyword_id < 0 || keyword_id >= KWS_KEYWORDS) return "unknown";
    return s_kw_names[keyword_id];
}

esp_err_t keyword_spotter_init(const char *templates_dir)
{
    /* Inicializa tabelas FFT (ignora REINITIALIZED — pode já estar pronto) */
    esp_err_t fft_err = dsps_fft2r_init_fc32(NULL, KWS_FFT_SIZE);
    if (fft_err != ESP_OK && fft_err != ESP_ERR_DSP_REINITIALIZED) {
        ESP_LOGE(TAG, "dsps_fft2r_init_fc32 falhou: %d", fft_err);
        return fft_err;
    }

    /* Aloca buffers em PSRAM */
    s_templates = heap_caps_malloc(KWS_MAX_TEMPLATES * sizeof(kws_template_t),
                                   MALLOC_CAP_SPIRAM);
    if (!s_templates) {
        ESP_LOGE(TAG, "sem PSRAM para templates (%u bytes)",
                 (unsigned)(KWS_MAX_TEMPLATES * sizeof(kws_template_t)));
        return ESP_ERR_NO_MEM;
    }
    memset(s_templates, 0, KWS_MAX_TEMPLATES * sizeof(kws_template_t));

    s_query_mfcc = heap_caps_malloc(
        (size_t)KWS_MAX_QUERY_FRAMES * KWS_N_MFCC * sizeof(float),
        MALLOC_CAP_SPIRAM);
    if (!s_query_mfcc) {
        ESP_LOGE(TAG, "sem PSRAM para query_mfcc");
        heap_caps_free(s_templates); s_templates = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_fft_buf = heap_caps_malloc(KWS_FFT_SIZE * 2 * sizeof(float), MALLOC_CAP_SPIRAM);
    if (!s_fft_buf) {
        ESP_LOGE(TAG, "sem PSRAM para fft_buf");
        heap_caps_free(s_query_mfcc); s_query_mfcc = NULL;
        heap_caps_free(s_templates);  s_templates  = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Pré-computa tabelas */
    init_hann();
    init_mel_bins();
    init_dct_matrix();

    /* Buffer temporário para leitura de WAV (1.5s = 24000 samples) */
    int16_t *wav_buf = heap_caps_malloc(KWS_MAX_FRAMES * KWS_HOP_SAMPLES * sizeof(int16_t),
                                         MALLOC_CAP_SPIRAM);
    if (!wav_buf) {
        ESP_LOGE(TAG, "sem PSRAM para wav_buf");
        heap_caps_free(s_fft_buf);    s_fft_buf    = NULL;
        heap_caps_free(s_query_mfcc); s_query_mfcc = NULL;
        heap_caps_free(s_templates);  s_templates  = NULL;
        return ESP_ERR_NO_MEM;
    }

    /* Carrega templates do SD */
    s_n_templates = 0;
    char path[128];

    for (int kw = 0; kw < KWS_KEYWORDS; kw++) {
        for (int i = 0; i < KWS_N_TPL_PER_KW; i++) {
            snprintf(path, sizeof(path), "%s/%s_%d.wav",
                     templates_dir, s_kw_names[kw], i);

            /* Máximo de samples para o buffer temporário */
            size_t max_wav = KWS_MAX_FRAMES * KWS_HOP_SAMPLES;
            size_t n = load_wav(path, wav_buf, max_wav);
            if (n < (size_t)KWS_FRAME_SAMPLES) {
                ESP_LOGD(TAG, "template ausente: %s", path);
                continue;
            }

            kws_template_t *tpl = &s_templates[s_n_templates];
            tpl->n_frames   = (uint16_t)extract_mfcc(wav_buf, n,
                                                       (float *)tpl->mfcc,
                                                       KWS_MAX_FRAMES);
            tpl->keyword_id = (uint8_t)kw;
            tpl->valid      = (tpl->n_frames >= KWS_MIN_MATCH_FRAMES);

            if (tpl->valid) {
                ESP_LOGD(TAG, "tpl[%d] kw=%s i=%d frames=%u",
                         s_n_templates, s_kw_names[kw], i, tpl->n_frames);
                s_n_templates++;
            }
        }
    }

    heap_caps_free(wav_buf);

    if (s_n_templates == 0) {
        ESP_LOGW(TAG, "nenhum template carregado de '%s'", templates_dir);
        /* Não desaloca — match() retornará KWS_NO_MATCH */
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ok — %d/%d templates, PSRAM ~%uKB",
             s_n_templates, KWS_MAX_TEMPLATES,
             (unsigned)((KWS_MAX_TEMPLATES * sizeof(kws_template_t)
                         + (size_t)KWS_MAX_QUERY_FRAMES * KWS_N_MFCC * sizeof(float)
                         + KWS_FFT_SIZE * 2 * sizeof(float)) / 1024u));
    return ESP_OK;
}

kws_result_t keyword_spotter_match(const int16_t *audio, size_t samples)
{
    kws_result_t result = { .keyword_id = KWS_NO_MATCH, .confidence = 0.0f };

    if (!s_templates || !s_query_mfcc || !s_fft_buf || s_n_templates == 0) {
        return result;
    }

    /* Extrai MFCCs da query */
    int n_query = extract_mfcc(audio, samples, s_query_mfcc, KWS_MAX_QUERY_FRAMES);
    if (n_query < KWS_MIN_MATCH_FRAMES) {
        ESP_LOGD(TAG, "query muito curta (%d frames)", n_query);
        return result;
    }

    /* Compara contra todos os templates */
    float best_dist = KWS_MATCH_THRESHOLD;
    float real_min  = FLT_MAX;
    int   real_min_kw = KWS_NO_MATCH;
    int   best_id   = KWS_NO_MATCH;

    /* Acumula distância mínima por keyword para log compacto */
    float kw_min[KWS_KEYWORDS];
    for (int i = 0; i < KWS_KEYWORDS; i++) kw_min[i] = FLT_MAX;

    for (int t = 0; t < s_n_templates; t++) {
        if (!s_templates[t].valid) continue;
        float dist = dtw_distance(
            s_query_mfcc,       n_query,
            (float *)s_templates[t].mfcc, (int)s_templates[t].n_frames);
        int kid = (int)s_templates[t].keyword_id;
        if (dist < kw_min[kid]) kw_min[kid] = dist;
        if (dist < real_min) { real_min = dist; real_min_kw = kid; }
        if (dist < best_dist) { best_dist = dist; best_id = kid; }
    }

    /* Log todas as distâncias por keyword (1 linha compacta) */
    for (int k = 0; k < KWS_KEYWORDS; k++) {
        if (kw_min[k] < FLT_MAX)
            ESP_LOGI(TAG, "  %-14s %.1f %s", s_kw_names[k], (double)kw_min[k],
                     kw_min[k] < KWS_MATCH_THRESHOLD ? "<-- MATCH" : "");
    }

    if (best_id != KWS_NO_MATCH) {
        result.keyword_id = best_id;
        result.confidence = 1.0f - best_dist / KWS_MATCH_THRESHOLD;
        ESP_LOGI(TAG, "match: %s  dist=%.3f  conf=%.2f",
                 s_kw_names[best_id], (double)best_dist, (double)result.confidence);
    } else {
        ESP_LOGI(TAG, "no match  real_min=%.3f(%s)  thr=%.1f  frames=%d",
                 (double)real_min,
                 real_min_kw >= 0 ? s_kw_names[real_min_kw] : "?",
                 (double)KWS_MATCH_THRESHOLD, n_query);
    }

    return result;
}
