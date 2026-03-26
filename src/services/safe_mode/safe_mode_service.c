#include "safe_mode_service.h"
#include "config_manager.h"
#include "emotion_mapper.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SAFE_MODE";

static bool s_active = false;

/* ── Timer de estabilidade (60 s) ──────────────────────────────────── */

static void stable_timer_cb(void *arg)
{
    config_set_int("boot_cnt", 0);
    ESP_LOGI(TAG, "sistema estável por %d s — boot_cnt resetado",
             SAFE_MODE_STABLE_S);
}

/* ── API pública ───────────────────────────────────────────────────── */

void safe_mode_check(void)
{
    /* (1) Limpa flag de boot não limpo deixada pelo shutdown handler */
    int32_t unclean = config_get_int("unclean_boot", 0);
    if (unclean) {
        config_set_int("unclean_boot", 0);
        ESP_LOGW(TAG, "boot anterior foi não limpo (brownout/crash)");
    }

    /* (2) Incrementa contagem de boots consecutivos */
    int32_t cnt = config_get_int("boot_cnt", 0);
    cnt++;
    config_set_int("boot_cnt", cnt);
    ESP_LOGI(TAG, "boot_cnt=%ld", (long)cnt);

    /* (3) Limiar superado → safe mode */
    if (cnt >= SAFE_MODE_BOOT_THRESH) {
        s_active = true;

        ESP_LOGW(TAG, "SAFE MODE ativo (boot_cnt=%ld >= %d)",
                 (long)cnt, SAFE_MODE_BOOT_THRESH);

        /* LEDs externos controlados pelo animation system via safe_mode_is_active() */
        emotion_mapper_apply(EMOTION_NEUTRAL, 0);
    }

    /* (4) Sempre agenda o timer de estabilidade.
     * Em safe mode: ao expirar, zera boot_cnt para que o próximo boot
     * saia do safe mode. Em modo normal: confirma estabilidade. */
    esp_timer_handle_t timer;
    const esp_timer_create_args_t args = {
        .callback        = stable_timer_cb,
        .arg             = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name            = "safe_mode_stable",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_once(timer,
                                         (uint64_t)SAFE_MODE_STABLE_S
                                         * 1000000ULL));
}

bool safe_mode_is_active(void)
{
    return s_active;
}
