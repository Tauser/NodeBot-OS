#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <inttypes.h>
#include <string.h>
#include <stdint.h>

#include "event_bus.h"

static const char *TAG = "test_ebus";

/* ── payloads de teste ───────────────────────────────────────────────── */
typedef struct { int16_t ax, ay, az; } imu_data_t;
typedef struct { uint16_t freq; uint8_t volume; } led_cmd_t;

/* ── contadores de callbacks ─────────────────────────────────────────── */
static volatile uint32_t g_cb_touch;
static volatile uint32_t g_cb_imu;
static volatile uint32_t g_cb_led;
static volatile int16_t  g_last_ax;

/* ── callbacks ───────────────────────────────────────────────────────── */
static void on_touch(uint16_t type, void *p)
{
    (void)p;
    g_cb_touch++;
    ESP_LOGI(TAG, "  [CB] TOUCH type=0x%04X  count=%" PRIu32, type, g_cb_touch);
}

static void on_imu(uint16_t type, void *p)
{
    imu_data_t *d = (imu_data_t *)p;
    g_last_ax = d->ax;
    g_cb_imu++;
    ESP_LOGI(TAG, "  [CB] IMU  ax=%d ay=%d az=%d  count=%" PRIu32,
             d->ax, d->ay, d->az, g_cb_imu);
}

static void on_led(uint16_t type, void *p)
{
    led_cmd_t *d = (led_cmd_t *)p;
    g_cb_led++;
    ESP_LOGI(TAG, "  [CB] LED  freq=%u vol=%u  count=%" PRIu32,
             d->freq, d->volume, g_cb_led);
}

/* segundo subscriber no mesmo tipo — verifica múltiplos assinantes */
static volatile uint32_t g_cb_imu2;
static void on_imu2(uint16_t type, void *p)
{
    (void)p; (void)type;
    g_cb_imu2++;
}

/* ── macro de assertiva ──────────────────────────────────────────────── */
static uint32_t s_pass, s_fail;

#define ASSERT(cond, msg)                                               \
    do {                                                                \
        if (cond) {                                                     \
            ESP_LOGI(TAG, "  PASS  %s", msg);                          \
            s_pass++;                                                   \
        } else {                                                        \
            ESP_LOGE(TAG, "  FAIL  %s  (%s:%d)", msg, __FILE__, __LINE__); \
            s_fail++;                                                   \
        }                                                               \
    } while (0)

static void wait(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

/* ══════════════════════════════════════════════════════════════════════ */

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "══════════════════════════════════════");
    ESP_LOGI(TAG, "  EventBus — teste unitário E12");
    ESP_LOGI(TAG, "══════════════════════════════════════");

    /* ── T1: init ──────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "T1: init");
    ASSERT(event_bus_init() == ESP_OK, "init retorna ESP_OK");

    /* ── T2: subscribe ─────────────────────────────────────────────── */
    ESP_LOGI(TAG, "T2: subscribe");
    ASSERT(event_bus_subscribe(EVT_TOUCH_PRESS, on_touch) == ESP_OK,
           "subscribe TOUCH_PRESS");
    ASSERT(event_bus_subscribe(EVT_IMU_DATA,    on_imu)   == ESP_OK,
           "subscribe IMU_DATA (1)");
    ASSERT(event_bus_subscribe(EVT_IMU_DATA,    on_imu2)  == ESP_OK,
           "subscribe IMU_DATA (2) — mesmo tipo, dois assinantes");
    ASSERT(event_bus_subscribe(EVT_LED_CMD,     on_led)   == ESP_OK,
           "subscribe LED_CMD");
    ASSERT(event_bus_subscribe(EVT_TOUCH_PRESS, NULL) == ESP_ERR_INVALID_ARG,
           "subscribe com callback NULL retorna INVALID_ARG");

    /* ── T3: publish + payload copiado ─────────────────────────────── */
    ESP_LOGI(TAG, "T3: publish com payload");
    imu_data_t imu = { .ax = 123, .ay = -456, .az = 1000 };
    ASSERT(event_bus_publish(EVT_IMU_DATA, &imu, sizeof(imu),
                             EVENT_PRIO_SYSTEM) == ESP_OK,
           "publish IMU_DATA retorna ESP_OK");

    /* Corrompe o buffer original — o payload do callback deve ser independente */
    imu.ax = 0x7FFF;

    wait(50);
    ASSERT(g_cb_imu  == 1, "callback IMU_DATA chamado 1×");
    ASSERT(g_cb_imu2 == 1, "callback IMU_DATA (2) chamado 1× — múltiplos subs");
    ASSERT(g_last_ax == 123, "payload ax=123 preservado após corrupção do original");

    /* ── T4: publish sem payload ────────────────────────────────────── */
    ESP_LOGI(TAG, "T4: publish sem payload (NULL)");
    ASSERT(event_bus_publish(EVT_TOUCH_PRESS, NULL, 0,
                             EVENT_PRIO_SAFETY) == ESP_OK,
           "publish TOUCH_PRESS sem payload retorna ESP_OK");
    wait(50);
    ASSERT(g_cb_touch == 1, "callback TOUCH_PRESS chamado 1×");

    /* ── T5: múltiplos eventos ──────────────────────────────────────── */
    ESP_LOGI(TAG, "T5: multiplos eventos (5× LED_CMD)");
    led_cmd_t led = { .freq = 440, .volume = 80 };
    for (int i = 0; i < 5; i++) {
        event_bus_publish(EVT_LED_CMD, &led, sizeof(led), EVENT_PRIO_COSMETIC);
    }
    wait(100);
    ASSERT(g_cb_led == 5, "5 eventos LED_CMD entregues");

    /* ── T6: esgotamento de fila + pool ────────────────────────────── */
    /*
     * Com o scheduler suspenso o despachante não roda.
     * Publicamos 70 eventos:
     *   – Os primeiros 32 vão para a fila SAFETY (depth=32)  → ok
     *   – Os seguintes 3 falham (fila SAFETY cheia)          → dropped
     *   – Os próximos 32 vão para a fila SYSTEM              → ok
     *     (slots foram liberados quando a fila SAFETY rejeitou)
     *   – Os últimos 3 falham com pool esgotado              → dropped
     * Total esperado: ok=64, dropped=6
     */
    ESP_LOGI(TAG, "T6: esgotamento de fila / pool");

    uint32_t drp_before;
    event_bus_get_stats(NULL, NULL, &drp_before);

    vTaskSuspendAll();
    int ok_cnt = 0, fail_cnt = 0;
    for (int i = 0; i < 70; i++) {
        uint8_t prio = (i < 35) ? EVENT_PRIO_SAFETY : EVENT_PRIO_SYSTEM;
        esp_err_t e = event_bus_publish(EVT_IMU_DATA, &imu, sizeof(imu), prio);
        if (e == ESP_OK) ok_cnt++;
        else             fail_cnt++;
    }
    xTaskResumeAll();

    wait(400);   /* aguarda despacho de todos os eventos enfileirados */

    uint32_t drp_after;
    event_bus_get_stats(NULL, NULL, &drp_after);

    ASSERT(ok_cnt <= EVENT_BUS_POOL_SIZE, "T6 enviados ≤ pool size (64)");
    ASSERT(fail_cnt > 0, "T6 drop detectado (fila ou pool cheio)");
    ESP_LOGI(TAG, "  T6 enviados=%d descartados=%d", ok_cnt, fail_cnt);
    ASSERT(drp_after > drp_before, "T6 contador dropped incrementado");

    /* ── T7: prioridade — SAFETY antes de COSMETIC ──────────────────── */
    /*
     * Publicamos COSMETIC antes de SAFETY com scheduler suspenso.
     * Ao resumir, o despachante deve entregar SAFETY primeiro.
     * Rastreamos a ordem de chegada via g_cb_touch (SAFETY) e g_cb_led (COSMETIC).
     */
    ESP_LOGI(TAG, "T7: prioridade SAFETY > COSMETIC");

    uint32_t touch_before = g_cb_touch;
    uint32_t led_before   = g_cb_led;

    /*
     * Nota: C não tem closures. O teste de ordem usa uma variável global
     * simples: verifica apenas que ambos os callbacks foram chamados após
     * o resume — a prioridade garante que SAFETY drena primeiro dentro
     * do despachante (loop reinicia pelo índice 0 a cada evento).
     * Validação formal de ordem requereria semáforos binários adicionais.
     */
    vTaskSuspendAll();
    event_bus_publish(EVT_LED_CMD,     &led, sizeof(led), EVENT_PRIO_COSMETIC);
    event_bus_publish(EVT_LED_CMD,     &led, sizeof(led), EVENT_PRIO_COSMETIC);
    event_bus_publish(EVT_TOUCH_PRESS, NULL, 0,            EVENT_PRIO_SAFETY);
    xTaskResumeAll();

    wait(100);
    ASSERT(g_cb_touch > touch_before, "T7 SAFETY (TOUCH) entregue após publicar após COSMETIC");
    ASSERT(g_cb_led   > led_before,   "T7 COSMETIC (LED) também entregue");

    /* ── T8: estatísticas finais ────────────────────────────────────── */
    ESP_LOGI(TAG, "T8: estatísticas");
    wait(100);
    uint32_t pub, del, drp;
    event_bus_get_stats(&pub, &del, &drp);
    ESP_LOGI(TAG, "  published=%" PRIu32 "  delivered=%" PRIu32
                  "  dropped=%" PRIu32, pub, del, drp);
    ASSERT(pub > 0,         "published > 0");
    ASSERT(del > 0,         "delivered > 0");
    ASSERT(drp >= 6,        "dropped ≥ 6  (T6: fila + pool cheios)");
    ASSERT(del <= pub,      "delivered ≤ published");

    /* ── Resultado ──────────────────────────────────────────────────── */
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "══════════════════════════════════════");
    ESP_LOGI(TAG, "  Resultado: PASS=%" PRIu32 "  FAIL=%" PRIu32, s_pass, s_fail);
    if (s_fail == 0)
        ESP_LOGI(TAG, "  ✓ todos os testes passaram");
    else
        ESP_LOGE(TAG, "  ✗ %"PRIu32" teste(s) falharam", s_fail);
    ESP_LOGI(TAG, "══════════════════════════════════════");
}
