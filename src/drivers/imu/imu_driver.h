#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IMU_TYPE_UNKNOWN  = 0,
    IMU_TYPE_MPU6050  = 1,
    IMU_TYPE_ICM42688 = 2,
} imu_type_t;

/*
 * Inicializa o barramento I2C e detecta o IMU (MPU-6050 ou ICM-42688).
 * Configura: accel ±4g, gyro ±500dps, ODR 100Hz.
 * Retorna ESP_ERR_NOT_FOUND se nenhum IMU for detectado.
 */
esp_err_t imu_init(void);

/* Retorna o tipo de IMU detectado. */
imu_type_t imu_get_type(void);

/*
 * Lê aceleração nos 3 eixos em milli-g.
 * Em repouso flat: |x|≈0, |y|≈0, |z|≈1000 mg.
 * magnitude √(x²+y²+z²) ≈ 1000 mg.
 */
esp_err_t imu_get_accel_mg(int16_t *x, int16_t *y, int16_t *z);

/*
 * Lê velocidade angular nos 3 eixos em décimos de grau/s (×10 dps).
 * Em repouso: valores próximos de zero.
 */
esp_err_t imu_get_gyro_dps(int16_t *x, int16_t *y, int16_t *z);

#ifdef __cplusplus
}
#endif
