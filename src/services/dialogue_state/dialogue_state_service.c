#include "dialogue_state_service.h"
#include "tts.h"
#include "intent_mapper.h"
#include "event_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <time.h>

static const char *TAG = "dialogue";

/* ── Timeouts ──────────────────────────────────────────────────────────── */
#define LISTENING_TIMEOUT_US   (3000000LL)   /* 3 s */
#define PROCESSING_TIMEOUT_US  (5000000LL)   /* 5 s */

/* ── Estado ────────────────────────────────────────────────────────────── */
static volatile dialogue_state_t s_state = DIALOGUE_IDLE;
static SemaphoreHandle_t         s_mutex = NULL;

static esp_timer_handle_t s_listen_timer    = NULL;
static esp_timer_handle_t s_processing_timer = NULL;

/* ── Mapeamento intent → frase ─────────────────────────────────────────── */
static phrase_id_t intent_to_phrase(intent_t intent)
{
    switch (intent) {
        case INTENT_SLEEP:        return PHRASE_SLEEP;
        case INTENT_WAKE:         return PHRASE_WAKE;
        case INTENT_SILENCE:      return PHRASE_SILENCE;
        case INTENT_HOW_ARE_YOU:  return PHRASE_HOW_ARE_YOU;
        case INTENT_LOOK_AT_ME:   return PHRASE_LOOK_AT_ME;
        case INTENT_VOLUME_UP:    return PHRASE_VOLUME_UP;
        case INTENT_VOLUME_DOWN:  return PHRASE_VOLUME_DOWN;
        case INTENT_YES:          return PHRASE_YES;
        case INTENT_NO:           return PHRASE_NO;
        case INTENT_CANCEL:       return PHRASE_CANCEL;
        case INTENT_WHAT_TIME:
        case INTENT_PRIVACY_MODE:
        default:                  return PHRASE_NOT_UNDERSTOOD;
    }
}

/* ── Helpers de transição ──────────────────────────────────────────────── */

static void publish_state(dialogue_state_t st)
{
    dialogue_state_event_t ev = { .state = (uint8_t)st };
    event_bus_publish(EVT_DIALOGUE_STATE_CHANGED, &ev, sizeof(ev),
                      EVENT_PRIO_BEHAVIOR);
    ESP_LOGD(TAG, "state → %d", (int)st);
}

static void enter_idle(phrase_id_t fallback)
{
    esp_timer_stop(s_listen_timer);
    esp_timer_stop(s_processing_timer);
    s_state = DIALOGUE_IDLE;
    publish_state(DIALOGUE_IDLE);
    if (fallback < PHRASE_COUNT) tts_play_phrase(fallback);
}

/* Transita para SPEAKING e publica estado. Chamado sem mutex. */
static void enter_speaking(void)
{
    esp_timer_stop(s_processing_timer);
    s_state = DIALOGUE_SPEAKING;
    publish_state(DIALOGUE_SPEAKING);
}

/* ── Callbacks dos timers ──────────────────────────────────────────────── */

static void on_listen_timeout(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_state == DIALOGUE_LISTENING) {
        ESP_LOGI(TAG, "listen timeout → IDLE");
        enter_idle(PHRASE_TIMEOUT_LISTENING);
    }
    xSemaphoreGive(s_mutex);
}

static void on_processing_timeout(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_state != DIALOGUE_PROCESSING) { xSemaphoreGive(s_mutex); return; }
    ESP_LOGI(TAG, "processing timeout → IDLE");
    enter_idle(PHRASE_NOT_UNDERSTOOD);
    xSemaphoreGive(s_mutex);
}

/* ── Callbacks do EventBus ─────────────────────────────────────────────── */

static void on_wake_word(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_state != DIALOGUE_IDLE) {
        /* Wake word durante fala ou processamento: ignorar */
        xSemaphoreGive(s_mutex);
        return;
    }

    esp_timer_stop(s_listen_timer);
    s_state = DIALOGUE_LISTENING;
    publish_state(DIALOGUE_LISTENING);
    esp_timer_start_once(s_listen_timer, LISTENING_TIMEOUT_US);
    ESP_LOGI(TAG, "IDLE → LISTENING");

    xSemaphoreGive(s_mutex);
}

static void on_intent_detected(uint16_t type, void *payload)
{
    (void)type;
    const intent_event_t *ev = (const intent_event_t *)payload;
    if (!ev) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_state != DIALOGUE_LISTENING) {
        xSemaphoreGive(s_mutex);
        return;
    }

    esp_timer_stop(s_listen_timer);
    s_state = DIALOGUE_PROCESSING;
    publish_state(DIALOGUE_PROCESSING);
    esp_timer_start_once(s_processing_timer, PROCESSING_TIMEOUT_US);
    ESP_LOGI(TAG, "LISTENING → PROCESSING  intent=%d conf=%d",
             (int)ev->intent, (int)ev->confidence);

    /* Determina resposta e enfileira TTS */
    intent_t intent = (intent_t)ev->intent;

    if (intent == INTENT_WHAT_TIME) {
        /* Template dinâmico: hora atual */
        time_t now;
        struct tm tm_info;
        time(&now);
        localtime_r(&now, &tm_info);
        tts_play_dynamic(TPL_HOUR, tm_info.tm_hour);
    } else {
        phrase_id_t phrase = intent_to_phrase(intent);
        tts_play_phrase(phrase);
    }

    /* PROCESSING → SPEAKING (temos resposta) */
    enter_speaking();
    ESP_LOGI(TAG, "PROCESSING → SPEAKING");

    xSemaphoreGive(s_mutex);
}

static void on_tts_done(uint16_t type, void *payload)
{
    (void)type; (void)payload;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_state != DIALOGUE_SPEAKING) {
        xSemaphoreGive(s_mutex);
        return;
    }

    s_state = DIALOGUE_IDLE;
    publish_state(DIALOGUE_IDLE);
    ESP_LOGI(TAG, "SPEAKING → IDLE");

    xSemaphoreGive(s_mutex);
}

/* ── API pública ────────────────────────────────────────────────────────── */

dialogue_state_t dialogue_state_get(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    dialogue_state_t st = s_state;
    xSemaphoreGive(s_mutex);
    return st;
}

esp_err_t dialogue_state_service_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "falha ao criar mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Timers de segurança */
    const esp_timer_create_args_t listen_args = {
        .callback              = on_listen_timeout,
        .arg                   = NULL,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "dlg_listen",
        .skip_unhandled_events = false,
    };
    esp_err_t err = esp_timer_create(&listen_args, &s_listen_timer);
    if (err != ESP_OK) { vSemaphoreDelete(s_mutex); return err; }

    const esp_timer_create_args_t proc_args = {
        .callback              = on_processing_timeout,
        .arg                   = NULL,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "dlg_proc",
        .skip_unhandled_events = false,
    };
    err = esp_timer_create(&proc_args, &s_processing_timer);
    if (err != ESP_OK) {
        esp_timer_delete(s_listen_timer);
        vSemaphoreDelete(s_mutex);
        return err;
    }

    /* Subscrições */
    err  = event_bus_subscribe(EVT_WAKE_WORD,       on_wake_word);
    err |= event_bus_subscribe(EVT_INTENT_DETECTED, on_intent_detected);
    err |= event_bus_subscribe(EVT_TTS_DONE,        on_tts_done);
    if (err != ESP_OK) {
        esp_timer_delete(s_processing_timer);
        esp_timer_delete(s_listen_timer);
        vSemaphoreDelete(s_mutex);
        return err;
    }

    ESP_LOGI(TAG, "ok — IDLE, listen=%ds proc=%ds",
             (int)(LISTENING_TIMEOUT_US / 1000000LL),
             (int)(PROCESSING_TIMEOUT_US / 1000000LL));
    return ESP_OK;
}
