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
/* IMU */
#define EVT_IMU_DATA         0x0201   /* payload: { int16_t ax,ay,az,gx,gy,gz } */
#define EVT_IMU_FALL         0x0202   /* sem payload                             */
#define EVT_IMU_SHAKE        0x0203   /* payload: imu_motion_event_t             */
#define EVT_IMU_TILT         0x0204   /* payload: imu_motion_event_t             */
/* Áudio */
#define EVT_AUDIO_LEVEL      0x0301   /* payload: uint16_t rms_mg            */
#define EVT_AUDIO_KEYWORD    0x0302
#define EVT_VOICE_ACTIVITY   0x0303   /* payload: uint8_t active (1=on/0=off) */
#define EVT_WAKE_WORD        0x0304   /* sem payload                          */
/* LED */
#define EVT_LED_CMD          0x0401   /* payload: led_cmd_t                  */

/* idx=0xFF → set emotion color (LEDs 1+2 sincronizados)
 * idx=0..N → set pixel individual                       */
typedef struct {
    uint8_t r, g, b;
    uint8_t idx;
} led_cmd_t;
#define LED_CMD_EMOTION  0xFF   /* idx magic: set emotion color */
/* Display */
#define EVT_DISPLAY_CMD      0x0501
/* Gaze */
#define EVT_GAZE_UPDATE      0x0601   /* payload: gaze_event_t { float x, y } */
/* Servo / Safety */
#define EVT_SERVO_BLOCKED    0x0701   /* payload: uint8_t servo_id           */
/* Sistema */
#define EVT_SYS_LOWBAT       0x0F01
#define EVT_SYS_ERROR        0x0F02   /* payload: uint32_t error_code        */
#define EVT_HEALTH_CHANGED   0x0F03   /* payload: health_event_t             */

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
