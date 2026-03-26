#pragma once

/*
 * Versão do schema — incrementar ao adicionar, remover ou renomear chaves.
 * Uma mudança de versão aciona factory reset automático no próximo boot.
 */
#define CONFIG_SCHEMA_VERSION  2

/*
 * Número de chaves — deve ser igual ao número de entradas em CONFIG_DEFAULTS.
 * Usado para dimensionar o array de CRC.
 */
#define CONFIG_KEY_COUNT  13

/*
 * X(key, default_value)
 *
 * key:           string, máx 15 caracteres (limite NVS)
 * default_value: int32_t
 *
 * REGRAS:
 *   - Não reordenar entradas existentes — o CRC depende da ordem.
 *   - Para adicionar chave: append no fim + incrementar CONFIG_SCHEMA_VERSION
 *                           + incrementar CONFIG_KEY_COUNT.
 *   - Para remover chave:   remover + incrementar versão + decrementar count.
 */
#define CONFIG_DEFAULTS(X)                        \
    X("schema_ver",    CONFIG_SCHEMA_VERSION)     \
    X("led_bright",    128)   /* 0-255            */ \
    X("disp_bright",   200)   /* 0-255            */ \
    X("disp_timeout",  30)    /* segundos, 0=off  */ \
    X("touch_thr",     150)   /* % do baseline    */ \
    X("audio_vol",     80)    /* 0-100            */ \
    X("imu_odr",       100)   /* Hz: 50/100/200   */ \
    X("face_id",       0)     /* índice face ativa */ \
    X("wifi_en",       0)     /* 0=off 1=on       */ \
    X("ble_en",        0)     /* 0=off 1=on       */ \
    X("boot_cnt",      0)     /* contagem de boots consecutivos sem 60 s de estabilidade */ \
    X("crash_cnt",     0)     /* brownout / resets não limpos acumulados  */ \
    X("unclean_boot",  0)     /* 1 = brownout ocorreu no boot anterior    */
