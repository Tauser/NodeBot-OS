#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>

#include "imu_driver.h"

static const char *TAG = "imu_test";

void app_main(void)
{
    if (imu_init() != ESP_OK) {
        ESP_LOGE(TAG, "IMU não encontrado — verifique cabeamento I2C");
        return;
    }

    const char *name = (imu_get_type() == IMU_TYPE_MPU6050) ? "MPU-6050" : "ICM-42688";
    ESP_LOGI(TAG, "IMU: %s  — accel ±4g  gyro ±500dps  100Hz", name);

    while (1) {
        int16_t ax, ay, az;
        int16_t gx, gy, gz;

        if (imu_get_accel_mg(&ax, &ay, &az) == ESP_OK &&
            imu_get_gyro_dps(&gx, &gy, &gz) == ESP_OK) {

            /* magnitude em repouso deve ser ≈ 1000 mg */
            float mag = sqrtf((float)ax*ax + (float)ay*ay + (float)az*az);

            ESP_LOGI(TAG,
                     "A[mg] x=%4d y=%4d z=%4d  |a|=%5.0f  "
                     "G[dps×10] x=%5d y=%5d z=%5d",
                     ax, ay, az, mag, gx, gy, gz);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
