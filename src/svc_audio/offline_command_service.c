#include "offline_command_service.h"
#include "dialogue_state_service.h"
#include "speech_command_service.h"

#include "event_bus.h"
#include "wake_word.h"
#include "audio_feedback.h"
#include "audio_driver.h"
#include "config_manager.h"
#include "gaze_service.h"
#include "state_vector.h"
#include "behavior_engine.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "OFFLINE_CMD";

#define OFFLINE_CMD_MAX_TEXT 96

extern bool privacy_policy_set_active(bool active) __attribute__((weak));

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void facebench_reset_state(void)
{
    g_state.music_detected = false;
    g_state.person_present = false;
    g_state.being_touched = false;
    g_state.energy = 0.80f;
    g_state.mood_valence = 0.00f;
    g_state.mood_arousal = 0.20f;
    g_state.social_need = 0.00f;
    g_state.attention = 0.25f;
    g_state.comfort = 0.80f;
}

static bool facebench_apply(const char *mode)
{
    if (mode == NULL || mode[0] == '\0') {
        return false;
    }

    facebench_reset_state();

    if (strcmp(mode, "idle") == 0) {
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "engaged") == 0) {
        g_state.person_present = true;
        g_state.attention = 0.70f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "listen") == 0) {
        g_state.person_present = true;
        behavior_engine_request_wake();
        event_bus_publish(EVT_WAKE_WORD, NULL, 0u, EVENT_PRIO_SYSTEM);
        g_state.attention = 1.0f;
        return true;
    }

    if (strcmp(mode, "think") == 0) {
        g_state.person_present = true;
        behavior_engine_request_wake();
        event_bus_publish(EVT_WAKE_WORD, NULL, 0u, EVENT_PRIO_SYSTEM);
        g_state.attention = 0.88f;
        return true;
    }

    if (strcmp(mode, "happy") == 0) {
        g_state.person_present = true;
        g_state.attention = 0.68f;
        g_state.mood_valence = 0.42f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "glee") == 0) {
        g_state.person_present = true;
        g_state.attention = 0.72f;
        g_state.mood_valence = 0.72f;
        g_state.mood_arousal = 0.42f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "sad") == 0) {
        g_state.mood_valence = -0.32f;
        g_state.attention = 0.20f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "alert") == 0) {
        g_state.comfort = 0.05f;
        g_state.attention = 0.55f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "music") == 0) {
        g_state.music_detected = true;
        g_state.mood_valence = 0.36f;
        g_state.attention = 0.52f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "sleepy") == 0) {
        g_state.energy = 0.18f;
        g_state.attention = 0.10f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "unimp") == 0) {
        g_state.energy = 0.30f;
        g_state.attention = 0.16f;
        behavior_engine_request_wake();
        return true;
    }

    if (strcmp(mode, "sleep") == 0) {
        g_state.energy = 0.10f;
        g_state.attention = 0.0f;
        g_state.mood_arousal = 0.0f;
        g_state.person_present = false;
        g_state.music_detected = false;
        g_state.social_need = 0.0f;
        behavior_engine_request_sleep();
        return true;
    }

    return false;
}

static void trim_line(char *line)
{
    size_t len;

    if (line == NULL) {
        return;
    }

    len = strlen(line);
    while (len > 0u &&
           (line[len - 1u] == '\n' || line[len - 1u] == '\r' ||
            line[len - 1u] == ' '  || line[len - 1u] == '\t')) {
        line[--len] = '\0';
    }

    while (*line == ' ' || *line == '\t') {
        memmove(line, line + 1, strlen(line));
    }
}

static void normalize_text(const char *src, char *dst, size_t dst_size)
{
    size_t w = 0;
    bool prev_space = true;

    if (dst_size == 0u) {
        return;
    }

    while (*src != '\0' && w + 1u < dst_size) {
        unsigned char c = (unsigned char)*src++;

        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c - 'A' + 'a');
        }

        if (c == '\xc3') {
            unsigned char next = (unsigned char)*src;
            if (next != '\0') {
                src++;
                switch (next) {
                    case 0xa1: case 0xa0: case 0xa2: case 0xa3:
                    case 0x81: case 0x80: case 0x82: case 0x83:
                        c = 'a'; break;
                    case 0xa9: case 0xaa:
                    case 0x89: case 0x8a:
                        c = 'e'; break;
                    case 0xad: case 0x8d:
                        c = 'i'; break;
                    case 0xb3: case 0xb4: case 0xb5:
                    case 0x93: case 0x94: case 0x95:
                        c = 'o'; break;
                    case 0xba: case 0x9a:
                        c = 'u'; break;
                    case 0xa7: case 0x87:
                        c = 'c'; break;
                    default:
                        c = ' ';
                        break;
                }
            }
        }

        if (!isalnum(c)) {
            c = ' ';
        }

        if (c == ' ') {
            if (!prev_space) {
                dst[w++] = ' ';
            }
            prev_space = true;
            continue;
        }

        dst[w++] = (char)c;
        prev_space = false;
    }

    if (w > 0u && dst[w - 1u] == ' ') {
        w--;
    }

    dst[w] = '\0';
}

static bool is_token_boundary(char c)
{
    return (c == '\0' || c == ' ');
}

static bool text_has_phrase(const char *normalized, const char *pattern)
{
    size_t pattern_len;
    const char *pos;

    if (normalized == NULL || pattern == NULL || pattern[0] == '\0') {
        return false;
    }

    pattern_len = strlen(pattern);
    pos = normalized;

    while ((pos = strstr(pos, pattern)) != NULL) {
        char before = (pos == normalized) ? '\0' : pos[-1];
        char after = pos[pattern_len];
        if (is_token_boundary(before) && is_token_boundary(after)) {
            return true;
        }
        pos++;
    }

    return false;
}

const char *offline_intent_name(offline_intent_t intent)
{
    switch (intent) {
        case OFFLINE_INTENT_STOP:          return "STOP";
        case OFFLINE_INTENT_CANCEL:        return "CANCEL";
        case OFFLINE_INTENT_SILENCE:       return "SILENCE";
        case OFFLINE_INTENT_SLEEP:         return "SLEEP";
        case OFFLINE_INTENT_WAKE:          return "WAKE";
        case OFFLINE_INTENT_VOLUME_HIGH:   return "VOLUME_HIGH";
        case OFFLINE_INTENT_VOLUME_LOW:    return "VOLUME_LOW";
        case OFFLINE_INTENT_VOLUME_MEDIUM: return "VOLUME_MEDIUM";
        case OFFLINE_INTENT_LOOK_AT_ME:    return "LOOK_AT_ME";
        case OFFLINE_INTENT_GOOD_NIGHT:    return "GOOD_NIGHT";
        case OFFLINE_INTENT_PRIVACY_MODE:  return "PRIVACY_MODE";
        case OFFLINE_INTENT_YES:           return "YES";
        case OFFLINE_INTENT_NO:            return "NO";
        case OFFLINE_INTENT_OK:            return "OK";
        case OFFLINE_INTENT_UNKNOWN:
        default:                           return "UNKNOWN";
    }
}

bool offline_command_match_text(const char *text,
                                offline_intent_event_t *out_event)
{
    char normalized[OFFLINE_CMD_MAX_TEXT];
    offline_intent_t intent = OFFLINE_INTENT_UNKNOWN;

    if (text == NULL || out_event == NULL) {
        return false;
    }

    normalize_text(text, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return false;
    }

    if (text_has_phrase(normalized, "olha pra mim") ||
        text_has_phrase(normalized, "olha para mim") ||
        text_has_phrase(normalized, "me olha")) {
        intent = OFFLINE_INTENT_LOOK_AT_ME;
    } else if (text_has_phrase(normalized, "para") ||
               text_has_phrase(normalized, "pare")) {
        intent = OFFLINE_INTENT_STOP;
    } else if (text_has_phrase(normalized, "cancela") ||
               text_has_phrase(normalized, "cancelar")) {
        intent = OFFLINE_INTENT_CANCEL;
    } else if (text_has_phrase(normalized, "silencio agora") ||
               text_has_phrase(normalized, "fica em silencio") ||
               text_has_phrase(normalized, "silencio")) {
        intent = OFFLINE_INTENT_SILENCE;
    } else if (text_has_phrase(normalized, "boa noite")) {
        intent = OFFLINE_INTENT_GOOD_NIGHT;
    } else if (text_has_phrase(normalized, "vai dormir") ||
               text_has_phrase(normalized, "pode dormir") ||
               text_has_phrase(normalized, "dorme")) {
        intent = OFFLINE_INTENT_SLEEP;
    } else if (text_has_phrase(normalized, "pode acordar") ||
               text_has_phrase(normalized, "acorda")) {
        intent = OFFLINE_INTENT_WAKE;
    } else if (text_has_phrase(normalized, "volume alto")) {
        intent = OFFLINE_INTENT_VOLUME_HIGH;
    } else if (text_has_phrase(normalized, "volume baixo")) {
        intent = OFFLINE_INTENT_VOLUME_LOW;
    } else if (text_has_phrase(normalized, "volume medio")) {
        intent = OFFLINE_INTENT_VOLUME_MEDIUM;
    } else if (text_has_phrase(normalized, "modo privado")) {
        intent = OFFLINE_INTENT_PRIVACY_MODE;
    } else if (text_has_phrase(normalized, "sim")) {
        intent = OFFLINE_INTENT_YES;
    } else if (text_has_phrase(normalized, "nao")) {
        intent = OFFLINE_INTENT_NO;
    } else if (text_has_phrase(normalized, "ok")) {
        intent = OFFLINE_INTENT_OK;
    } else {
        return false;
    }

    out_event->intent = intent;
    out_event->timestamp_ms = now_ms();
    out_event->confidence_pct = 100u;
    return true;
}

static void handle_intent(const offline_intent_event_t *evt)
{
    switch (evt->intent) {
        case OFFLINE_INTENT_STOP:
        case OFFLINE_INTENT_CANCEL:
        case OFFLINE_INTENT_SILENCE:
            audio_set_volume(0u);
            config_set_int("audio_vol", 0);
            break;

        case OFFLINE_INTENT_VOLUME_HIGH:
            audio_set_volume(90u);
            config_set_int("audio_vol", 90);
            audio_feedback_play(SOUND_BEEP_ACK);
            break;

        case OFFLINE_INTENT_VOLUME_LOW:
            audio_set_volume(30u);
            config_set_int("audio_vol", 30);
            audio_feedback_play(SOUND_BEEP_ACK);
            break;

        case OFFLINE_INTENT_VOLUME_MEDIUM:
            audio_set_volume(60u);
            config_set_int("audio_vol", 60);
            audio_feedback_play(SOUND_BEEP_ACK);
            break;

        case OFFLINE_INTENT_LOOK_AT_ME:
            gaze_service_set_target(0.0f, 0.0f, 220u);
            audio_feedback_play(SOUND_DING_NOTIF);
            break;

        case OFFLINE_INTENT_SLEEP:
        case OFFLINE_INTENT_GOOD_NIGHT:
            g_state.energy = 0.10f;
            g_state.attention = 0.0f;
            g_state.mood_arousal = 0.0f;
            behavior_engine_request_sleep();
            audio_feedback_play(SOUND_BEEP_ACK);
            break;

        case OFFLINE_INTENT_WAKE:
            if (g_state.energy < 0.25f) {
                g_state.energy = 0.25f;
            }
            g_state.attention = 1.0f;
            g_state.mood_arousal = 0.25f;
            behavior_engine_request_wake();
            audio_feedback_play(SOUND_WHOOSH_ACTIVATE);
            break;

        case OFFLINE_INTENT_PRIVACY_MODE:
            if (privacy_policy_set_active) {
                privacy_policy_set_active(true);
            }
            audio_feedback_play(SOUND_ERROR_TONE);
            break;

        case OFFLINE_INTENT_YES:
        case OFFLINE_INTENT_NO:
        case OFFLINE_INTENT_OK:
            g_state.attention = 1.0f;
            audio_feedback_play(SOUND_BEEP_ACK);
            break;

        case OFFLINE_INTENT_UNKNOWN:
        default:
            break;
    }
}

bool offline_command_process_intent(offline_intent_t intent)
{
    offline_intent_event_t evt;

    if (intent <= OFFLINE_INTENT_UNKNOWN || intent >= OFFLINE_INTENT_OK + 1) {
        dialogue_state_service_report_no_understanding();
        return false;
    }

    evt.intent = intent;
    evt.timestamp_ms = now_ms();
    evt.confidence_pct = 100u;

    state_vector_on_voice_intent();
    handle_intent(&evt);
    event_bus_publish(EVT_OFFLINE_INTENT, &evt, sizeof(evt), EVENT_PRIO_SYSTEM);

    ESP_LOGI(TAG, "intent -> %s", offline_intent_name(evt.intent));
    return true;
}

bool offline_command_process_text(const char *text)
{
    offline_intent_event_t evt;

    if (!offline_command_match_text(text, &evt)) {
        dialogue_state_service_report_no_understanding();
        return false;
    }

    return offline_command_process_intent(evt.intent);
}

static void offline_command_serial_task(void *arg)
{
    char line[128];

    (void)arg;

    ESP_LOGI(TAG, "serial test ready  use: cmd <frase> | lang <off|en|ptbr> | facebench <mode>");

    for (;;) {
        char *input = NULL;

        if (!fgets(line, sizeof(line), stdin)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        if (strncmp(line, "lang", 4) == 0) {
            speech_command_language_t language = speech_command_service_get_language();
            char *arg_lang = line + 4;

            trim_line(arg_lang);
            if (arg_lang[0] == '\0') {
                ESP_LOGI(TAG, "speech language=%s",
                         speech_command_service_language_name(language));
                continue;
            }

            if (strcmp(arg_lang, "off") == 0) {
                language = SPEECH_CMD_LANG_OFF;
            } else if (strcmp(arg_lang, "en") == 0 || strcmp(arg_lang, "en-us") == 0) {
                language = SPEECH_CMD_LANG_EN_US;
            } else if (strcmp(arg_lang, "pt") == 0 || strcmp(arg_lang, "ptbr") == 0 ||
                       strcmp(arg_lang, "pt-br") == 0) {
                language = SPEECH_CMD_LANG_PT_BR;
            } else {
                ESP_LOGI(TAG, "invalid lang  use: off | en | ptbr");
                continue;
            }

            if (speech_command_service_set_language(language)) {
                ESP_LOGI(TAG, "speech language set -> %s",
                         speech_command_service_language_name(language));
            } else {
                ESP_LOGW(TAG, "speech language set with limited availability -> %s",
                         speech_command_service_language_name(language));
            }
            continue;
        } else if (strncmp(line, "facebench", 9) == 0) {
            char *arg_mode = line + 9;

            trim_line(arg_mode);
            if (arg_mode[0] == '\0') {
                ESP_LOGI(TAG,
                         "facebench modes: idle | engaged | listen | think | happy | glee | sad | alert | music | sleepy | unimp | sleep");
                continue;
            }

            if (facebench_apply(arg_mode)) {
                ESP_LOGI(TAG, "facebench -> %s", arg_mode);
            } else {
                ESP_LOGI(TAG,
                         "invalid facebench mode  use: idle | engaged | listen | think | happy | glee | sad | alert | music | sleepy | unimp | sleep");
            }
            continue;
        } else if (strncmp(line, "cmd ", 4) == 0) {
            input = line + 4;
        } else if (strncmp(line, "say ", 4) == 0) {
            input = line + 4;
        } else {
            continue;
        }

        trim_line(input);
        if (input[0] == '\0') {
            ESP_LOGI(TAG, "serial test ignored: empty command");
            continue;
        }

        ESP_LOGI(TAG, "serial test -> \"%s\"", input);
        offline_command_process_text(input);
    }
}

static void on_wake_word(uint16_t type, void *payload)
{
    (void)type;
    (void)payload;
    audio_feedback_play(SOUND_WHOOSH_ACTIVATE);
}

void offline_command_service_init(void)
{
    audio_set_volume((uint8_t)config_get_int("audio_vol", 80));
    event_bus_subscribe(EVT_WAKE_WORD, on_wake_word);
    xTaskCreatePinnedToCore(offline_command_serial_task, "OfflineCmdSerial",
                            4096, NULL, 3, NULL, 0);
    ESP_LOGI(TAG, "init  offline intents ready");
}
