#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa SDMMC slot 1 e monta FAT em "/sdcard". */
esp_err_t sd_init(void);

/* Escreve `len` bytes de `data` no arquivo `path` (cria ou sobrescreve). */
esp_err_t sd_write_file(const char *path, const void *data, size_t len);

/* Lê até `maxlen` bytes de `path` em `buf`. Retorna bytes lidos (0 = erro). */
size_t sd_read_file(const char *path, void *buf, size_t maxlen);

/* Retorna true se o arquivo existir. */
bool sd_file_exists(const char *path);

#ifdef __cplusplus
}
#endif
