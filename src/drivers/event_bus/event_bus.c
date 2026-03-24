#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>
#include <stdbool.h>

static const char *TAG = "event_bus";

/* ══════════════════════════════════════════════════════════════════════
   Pool de payloads — 64 slots × 64 B = 4 KB estáticos em SRAM
   Gerenciado por bitmask uint64_t: bit i = 1 → slot i livre
   ══════════════════════════════════════════════════════════════════════ */
_Static_assert(EVENT_BUS_POOL_SIZE <= 64,
               "EVENT_BUS_POOL_SIZE não pode ultrapassar 64 (limite do bitmask)");

typedef struct { uint8_t data[EVENT_BUS_PAYLOAD_SIZE]; } pool_slot_t;

static pool_slot_t  s_pool[EVENT_BUS_POOL_SIZE];
static uint64_t     s_free_mask;                        /* 1 = livre        */
static portMUX_TYPE s_pool_mux = portMUX_INITIALIZER_UNLOCKED;

static int pool_alloc(void)
{
    int idx = -1;
    taskENTER_CRITICAL(&s_pool_mux);
    if (s_free_mask) {
        idx = __builtin_ctzll(s_free_mask);             /* bit mais baixo   */
        s_free_mask &= ~(1ULL << idx);
    }
    taskEXIT_CRITICAL(&s_pool_mux);
    return idx;
}

static void pool_free(uint8_t idx)
{
    taskENTER_CRITICAL(&s_pool_mux);
    s_free_mask |= (1ULL << idx);
    taskEXIT_CRITICAL(&s_pool_mux);
}

/* ══════════════════════════════════════════════════════════════════════
   Tabela de assinantes — busca linear O(MAX_SUBS), suficiente para robô
   ══════════════════════════════════════════════════════════════════════ */
typedef struct {
    uint16_t type;
    void   (*cb)(uint16_t, void *);
} sub_t;

static sub_t             s_subs[EVENT_BUS_MAX_SUBS];
static uint8_t           s_nsubs;
static SemaphoreHandle_t s_sub_mtx;

/* ══════════════════════════════════════════════════════════════════════
   Filas de prioridade
   Índice: 0=SAFETY, 1=SYSTEM, 2=BEHAVIOR, 3=COSMETIC
   ══════════════════════════════════════════════════════════════════════ */
typedef struct {
    uint16_t type;
    uint8_t  slot;
    uint8_t  _pad;
} bus_item_t;

static QueueHandle_t s_q[4];

/* ══════════════════════════════════════════════════════════════════════
   Task despachante — notificada por xTaskNotifyGive (counting semaphore)
   ══════════════════════════════════════════════════════════════════════ */
static TaskHandle_t s_dispatcher;

/* ══════════════════════════════════════════════════════════════════════
   Estatísticas
   ══════════════════════════════════════════════════════════════════════ */
static uint32_t s_published;
static uint32_t s_delivered;
static uint32_t s_dropped;

/* ── helpers ─────────────────────────────────────────────────────────── */

static int prio_to_qidx(uint8_t p)
{
    if (p >= EVENT_PRIO_SAFETY)   return 0;
    if (p >= EVENT_PRIO_SYSTEM)   return 1;
    if (p >= EVENT_PRIO_BEHAVIOR) return 2;
    return 3;
}

static void dispatch_one(const bus_item_t *item)
{
    void *data = s_pool[item->slot].data;

    xSemaphoreTake(s_sub_mtx, portMAX_DELAY);
    uint8_t n = s_nsubs;
    for (uint8_t i = 0; i < n; i++) {
        if (s_subs[i].type == item->type) {
            s_subs[i].cb(item->type, data);
            taskENTER_CRITICAL(&s_pool_mux);
            s_delivered++;
            taskEXIT_CRITICAL(&s_pool_mux);
        }
    }
    xSemaphoreGive(s_sub_mtx);

    pool_free(item->slot);
}

/* ── task despachante ─────────────────────────────────────────────────
   Uma notificação por evento publicado (ulTaskNotifyTake com pdFALSE
   decrementa o contador em 1, comportamento de semáforo contador).
   A cada wake-up percorre as filas do mais prioritário para o menos,
   garantindo que SAFETY seja sempre servido antes de COSMETIC.
   ──────────────────────────────────────────────────────────────────── */
static void dispatcher_task(void *arg)
{
    (void)arg;
    bus_item_t item;

    for (;;) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);  /* decrementa count em 1 */

        bool found = false;
        for (int q = 0; q < 4; q++) {
            if (xQueueReceive(s_q[q], &item, 0) == pdTRUE) {
                dispatch_one(&item);
                found = true;
                break;   /* reinicia do topo (SAFETY) a cada evento */
            }
        }

        if (!found) {
            ESP_LOGW(TAG, "notificação sem item — ignorada");
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════
   API pública
   ══════════════════════════════════════════════════════════════════════ */

esp_err_t event_bus_init(void)
{
    /* Pool: todos os slots livres */
    s_free_mask = (EVENT_BUS_POOL_SIZE == 64)
                  ? ~0ULL
                  : (1ULL << EVENT_BUS_POOL_SIZE) - 1;
    s_nsubs     = 0;
    s_published = s_delivered = s_dropped = 0;

    s_sub_mtx = xSemaphoreCreateMutex();
    if (!s_sub_mtx) return ESP_ERR_NO_MEM;

    for (int i = 0; i < 4; i++) {
        s_q[i] = xQueueCreate(EVENT_BUS_QUEUE_DEPTH, sizeof(bus_item_t));
        if (!s_q[i]) {
            ESP_LOGE(TAG, "falha ao criar fila[%d]", i);
            return ESP_ERR_NO_MEM;
        }
    }

    BaseType_t r = xTaskCreate(dispatcher_task, "evt_dispatch",
                               3072, NULL,
                               18,          /* abaixo de watchdog, acima de tasks normais */
                               &s_dispatcher);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "falha ao criar task despachante");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "init OK  pool=%u B  slots=%u  subs_max=%u  q_depth=%u",
             (unsigned)(EVENT_BUS_POOL_SIZE * EVENT_BUS_PAYLOAD_SIZE),
             (unsigned)EVENT_BUS_POOL_SIZE,
             (unsigned)EVENT_BUS_MAX_SUBS,
             (unsigned)EVENT_BUS_QUEUE_DEPTH);
    return ESP_OK;
}

esp_err_t event_bus_publish(uint16_t type, void *payload, size_t len,
                             uint8_t priority)
{
    if (len > EVENT_BUS_PAYLOAD_SIZE) len = EVENT_BUS_PAYLOAD_SIZE;

    int slot = pool_alloc();
    if (slot < 0) {
        taskENTER_CRITICAL(&s_pool_mux);
        s_dropped++;
        taskEXIT_CRITICAL(&s_pool_mux);
        ESP_LOGD(TAG, "pool esgotado — 0x%04X descartado", type);
        return ESP_ERR_NO_MEM;
    }

    /* Copia payload; zera bytes restantes do slot */
    if (payload && len) {
        memcpy(s_pool[slot].data, payload, len);
        if (len < EVENT_BUS_PAYLOAD_SIZE)
            memset(s_pool[slot].data + len, 0, EVENT_BUS_PAYLOAD_SIZE - len);
    } else {
        memset(s_pool[slot].data, 0, EVENT_BUS_PAYLOAD_SIZE);
    }

    bus_item_t item = { .type = type, .slot = (uint8_t)slot };
    int q = prio_to_qidx(priority);

    if (xQueueSend(s_q[q], &item, 0) != pdTRUE) {
        pool_free((uint8_t)slot);
        taskENTER_CRITICAL(&s_pool_mux);
        s_dropped++;
        taskEXIT_CRITICAL(&s_pool_mux);
        ESP_LOGD(TAG, "fila[%d] cheia — 0x%04X descartado", q, type);
        return ESP_ERR_TIMEOUT;
    }

    taskENTER_CRITICAL(&s_pool_mux);
    s_published++;
    taskEXIT_CRITICAL(&s_pool_mux);

    xTaskNotifyGive(s_dispatcher);
    return ESP_OK;
}

esp_err_t event_bus_subscribe(uint16_t type,
                               void (*callback)(uint16_t, void *))
{
    if (!callback) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_sub_mtx, portMAX_DELAY);
    if (s_nsubs >= EVENT_BUS_MAX_SUBS) {
        xSemaphoreGive(s_sub_mtx);
        ESP_LOGE(TAG, "MAX_SUBS (%u) atingido", EVENT_BUS_MAX_SUBS);
        return ESP_ERR_NO_MEM;
    }
    s_subs[s_nsubs].type = type;
    s_subs[s_nsubs].cb   = callback;
    s_nsubs++;
    xSemaphoreGive(s_sub_mtx);
    return ESP_OK;
}

void event_bus_get_stats(uint32_t *published, uint32_t *delivered,
                          uint32_t *dropped)
{
    taskENTER_CRITICAL(&s_pool_mux);
    if (published) *published = s_published;
    if (delivered) *delivered = s_delivered;
    if (dropped)   *dropped   = s_dropped;
    taskEXIT_CRITICAL(&s_pool_mux);
}
