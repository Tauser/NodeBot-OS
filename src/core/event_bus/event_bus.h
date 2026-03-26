#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Dimensões ───────────────────────────────────────────────────────── */
#define EVENT_BUS_PAYLOAD_SIZE  64   /* bytes por slot (máx payload)      */
#define EVENT_BUS_POOL_SIZE     64   /* slots estáticos (≤ 64 — bitmask)  */
#define EVENT_BUS_MAX_SUBS      32   /* assinantes totais (todos os tipos) */
#define EVENT_BUS_QUEUE_DEPTH   32   /* itens por fila de prioridade       */

/* ── Prioridades de publicação ───────────────────────────────────────── */
#define EVENT_PRIO_SAFETY    20   /* fila SAFETY   — segurança crítica  */
#define EVENT_PRIO_SYSTEM    15   /* fila SYSTEM   — controle e estado  */
#define EVENT_PRIO_BEHAVIOR  10   /* fila BEHAVIOR — lógica de comportamento */
#define EVENT_PRIO_COSMETIC   5   /* fila COSMETIC — display, LEDs, efeitos  */

/* ── Tipos de evento (namespace por subsistema) ──────────────────────── */
/* Touch */
#define EVT_TOUCH_PRESS      0x0101
#define EVT_TOUCH_RELEASE    0x0102
#define EVT_TOUCH_DETECTED   0x0103  /* payload: touch_event_t (touch_service.h) */
/* IMU / Motion */
#define EVT_IMU_DATA         0x0201   /* payload: { int16_t ax,ay,az,gx,gy,gz } */
#define EVT_IMU_FALL         0x0202   /* legado — usar EVT_MOTION_DETECTED        */
#define EVT_MOTION_DETECTED  0x0203   /* payload: motion_event_t (imu_service.h) */
/* Áudio */
#define EVT_AUDIO_LEVEL      0x0301   /* payload: uint16_t rms_mg                    */
#define EVT_AUDIO_KEYWORD    0x0302
#define EVT_VOICE_ACTIVITY   0x0303   /* payload: voice_activity_event_t (vad.h)     */
/* LED */
#define EVT_LED_CMD          0x0401   /* payload: { uint8_t r,g,b,idx }      */
/* Display */
#define EVT_DISPLAY_CMD      0x0501
/* Gaze */
#define EVT_GAZE_UPDATE      0x0601   /* payload: gaze_event_t { float x, y } */
/* Wake word */
#define EVT_WAKE_WORD         0x0701  /* payload: wake_word_event_t (wake_word.h) */
/* Servo / Motion Safety */
#define EVT_SERVO_BLOCKED     0x0801  /* payload: servo_blocked_event_t      */
/* Sistema */
#define EVT_SYS_LOWBAT        0x0F01
#define EVT_SYS_ERROR         0x0F02  /* payload: uint32_t error_code        */
#define EVT_SERVICE_HEARTBEAT 0x0F10  /* payload: nenhum — keepalive         */

/* ── API ─────────────────────────────────────────────────────────────── */

/**
 * Inicializa o barramento de eventos.
 * Cria pool, filas FreeRTOS e task despachante (prioridade 18).
 * Deve ser chamado UMA VEZ em app_main antes de subscribe/publish.
 */
esp_err_t event_bus_init(void);

/**
 * Publica um evento.
 * O payload é copiado para um slot do pool — o buffer original pode ser
 * reutilizado logo após o retorno.
 *
 * @param type      Tipo do evento (EVT_*)
 * @param payload   Ponteiro para os dados (NULL → slot zerado)
 * @param len       Tamanho em bytes; truncado a EVENT_BUS_PAYLOAD_SIZE
 * @param priority  EVENT_PRIO_* — determina qual fila será usada
 *
 * @return ESP_OK            sucesso
 *         ESP_ERR_NO_MEM   pool de slots esgotado
 *         ESP_ERR_TIMEOUT  fila da prioridade cheia
 *
 * Seguro para chamada de qualquer task. NÃO chamar de ISR.
 */
esp_err_t event_bus_publish(uint16_t type, void *payload, size_t len,
                             uint8_t priority);

/**
 * Registra um callback para um tipo de evento.
 * Um mesmo tipo pode ter múltiplos callbacks (até MAX_SUBS total).
 * O callback é invocado na task despachante — deve ser curto e não bloquear.
 * O callback NÃO deve chamar event_bus_subscribe (deadlock).
 *
 * @return ESP_ERR_NO_MEM        limite MAX_SUBS atingido
 *         ESP_ERR_INVALID_ARG   callback NULL
 */
esp_err_t event_bus_subscribe(uint16_t type,
                               void (*callback)(uint16_t type, void *payload));

/**
 * Retorna contadores acumulados desde o init.
 * Qualquer ponteiro NULL é ignorado.
 */
void event_bus_get_stats(uint32_t *published, uint32_t *delivered,
                          uint32_t *dropped);

#ifdef __cplusplus
}
#endif
