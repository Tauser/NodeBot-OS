#include "imu_driver.h"
#include "hal_init.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include <math.h>
#include <string.h>

static const char *TAG = "imu";

#define IMU_ADDR        0x68
#define I2C_TIMEOUT_MS  50

/* ── WHO_AM_I ────────────────────────────────────────────────────── */
#define REG_WHO_AM_I        0x75
#define WHOAMI_MPU6050      0x68
#define WHOAMI_ICM42688     0x47

/* ── MPU-6050 registradores ──────────────────────────────────────── */
#define MPU_SMPLRT_DIV      0x19   /* 1000/(1+9) = 100 Hz           */
#define MPU_CONFIG          0x1A   /* DLPF_CFG=1 → 188 Hz BW        */
#define MPU_GYRO_CONFIG     0x1B   /* FS_SEL=1 → ±500 dps           */
#define MPU_ACCEL_CONFIG    0x1C   /* AFS_SEL=1 → ±4g               */
#define MPU_ACCEL_XOUT_H    0x3B
#define MPU_GYRO_XOUT_H     0x43
#define MPU_PWR_MGMT_1      0x6B

/* ── ICM-42688 registradores ─────────────────────────────────────── */
#define ICM_PWR_MGMT0       0x4E
#define ICM_GYRO_CONFIG0    0x4F   /* FS_SEL | ODR                  */
#define ICM_ACCEL_CONFIG0   0x50
#define ICM_ACCEL_XOUT_H    0x1F  /* inicia bloco: temp(2)+accel(6)+gyro(6) */
#define ICM_ACCEL_DATA_X1   0x1D

/* sensibilidades ---------------------------------------------------- */
/* ±4g  → 8192 LSB/g  → mg = raw * 1000 / 8192                       */
/* ±500dps → 65.5 LSB/dps → dps×10 = raw * 10 / 65 (inteiro)         */
#define ACCEL_SENS_DIV  8192
#define GYRO_SENS_DIV   655   /* ×10 para evitar float: raw*10/655 */

/* ── estado ──────────────────────────────────────────────────────── */

static i2c_master_bus_handle_t  s_bus;
static i2c_master_dev_handle_t  s_dev;
static imu_type_t               s_type = IMU_TYPE_UNKNOWN;

/* ── helpers I2C ─────────────────────────────────────────────────── */

static esp_err_t imu_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, 2, I2C_TIMEOUT_MS);
}

static esp_err_t imu_read(uint8_t reg, uint8_t *data, size_t len)
{
    /* 3 tentativas — barramento compartilhado com OV2640 causa falhas esporádicas */
    for (int attempt = 0; attempt < 3; attempt++) {
        esp_err_t ret = i2c_master_transmit_receive(s_dev, &reg, 1, data, len, I2C_TIMEOUT_MS);
        if (ret == ESP_OK) return ESP_OK;
        if (attempt < 2) vTaskDelay(pdMS_TO_TICKS(2));
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t imu_read_u8(uint8_t reg, uint8_t *val)
{
    return imu_read(reg, val, 1);
}

/* ── init por chip ───────────────────────────────────────────────── */

static esp_err_t init_mpu6050(void)
{
    ESP_ERROR_CHECK(imu_write(MPU_PWR_MGMT_1,   0x80));  /* reset     */
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(imu_write(MPU_PWR_MGMT_1,   0x01));  /* clk PLL   */
    ESP_ERROR_CHECK(imu_write(MPU_SMPLRT_DIV,   0x09));  /* 100 Hz    */
    ESP_ERROR_CHECK(imu_write(MPU_CONFIG,        0x01));  /* DLPF 188Hz*/
    ESP_ERROR_CHECK(imu_write(MPU_GYRO_CONFIG,   0x08));  /* ±500 dps  */
    ESP_ERROR_CHECK(imu_write(MPU_ACCEL_CONFIG,  0x08));  /* ±4g       */
    return ESP_OK;
}

static esp_err_t init_icm42688(void)
{
    /* Reset via soft reset bit */
    ESP_ERROR_CHECK(imu_write(0x11, 0x01));               /* DEVICE_CONFIG reset */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Accel ±4g (FS=2) ODR 100Hz (0x08) → 0x28 | 0x08 = 0x28 */
    ESP_ERROR_CHECK(imu_write(ICM_ACCEL_CONFIG0, 0x28));  /* FS=2(±4g)  ODR=8(100Hz) */

    /* Gyro ±500dps (FS=2) ODR 100Hz (0x08) → 0x20 | 0x08 = 0x28 */
    ESP_ERROR_CHECK(imu_write(ICM_GYRO_CONFIG0,  0x28));  /* FS=2(±500) ODR=8(100Hz) */

    /* Accel+Gyro LN mode ON */
    ESP_ERROR_CHECK(imu_write(ICM_PWR_MGMT0,     0x0F));
    vTaskDelay(pdMS_TO_TICKS(50));
    return ESP_OK;
}

/* ── public ──────────────────────────────────────────────────────── */

esp_err_t imu_init(void)
{
    /* Inicializa barramento I2C */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = HAL_I2C_PORT,
        .sda_io_num          = HAL_I2C_SDA,
        .scl_io_num          = HAL_I2C_SCL,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = IMU_ADDR,
        .scl_speed_hz    = HAL_I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev));

    /* Auto-detecção pelo WHO_AM_I */
    uint8_t who = 0;
    esp_err_t ret = imu_read_u8(REG_WHO_AM_I, &who);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C sem resposta em 0x%02X", IMU_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    if (who == WHOAMI_MPU6050) {
        s_type = IMU_TYPE_MPU6050;
        ESP_LOGI(TAG, "MPU-6050 detectado (0x%02X)", who);
        return init_mpu6050();
    } else if (who == WHOAMI_ICM42688) {
        s_type = IMU_TYPE_ICM42688;
        ESP_LOGI(TAG, "ICM-42688 detectado (0x%02X)", who);
        return init_icm42688();
    }

    ESP_LOGE(TAG, "IMU desconhecido WHO_AM_I=0x%02X", who);
    return ESP_ERR_NOT_FOUND;
}

imu_type_t imu_get_type(void) { return s_type; }

/* ── leitura ─────────────────────────────────────────────────────── */

static inline int16_t to_s16(uint8_t hi, uint8_t lo)
{
    return (int16_t)((hi << 8) | lo);
}

esp_err_t imu_get_accel_mg(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t raw[6];

    if (s_type == IMU_TYPE_MPU6050) {
        ESP_RETURN_ON_ERROR(imu_read(MPU_ACCEL_XOUT_H, raw, 6), TAG, "accel read");
    } else {
        /* ICM-42688: registradores ACCEL_DATA_X1..Z0 em 0x1F–0x24 */
        uint8_t reg = 0x1F;
        ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_dev, &reg, 1, raw, 6,
                                                         I2C_TIMEOUT_MS), TAG, "accel read");
    }

    int16_t rx = to_s16(raw[0], raw[1]);
    int16_t ry = to_s16(raw[2], raw[3]);
    int16_t rz = to_s16(raw[4], raw[5]);

    /* X e Y trocados conforme orientação física da placa */
    *x = (int16_t)((int32_t)ry * 1000 / ACCEL_SENS_DIV);
    *y = (int16_t)((int32_t)rx * 1000 / ACCEL_SENS_DIV);
    *z = (int16_t)((int32_t)rz * 1000 / ACCEL_SENS_DIV);
    return ESP_OK;
}

esp_err_t imu_get_gyro_dps(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t raw[6];

    if (s_type == IMU_TYPE_MPU6050) {
        ESP_RETURN_ON_ERROR(imu_read(MPU_GYRO_XOUT_H, raw, 6), TAG, "gyro read");
    } else {
        /* ICM-42688: GYRO_DATA_X1..Z0 em 0x25–0x2A */
        uint8_t reg = 0x25;
        ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(s_dev, &reg, 1, raw, 6,
                                                         I2C_TIMEOUT_MS), TAG, "gyro read");
    }

    int16_t rx = to_s16(raw[0], raw[1]);
    int16_t ry = to_s16(raw[2], raw[3]);
    int16_t rz = to_s16(raw[4], raw[5]);

    /* resultado em dps×10; X e Y trocados conforme orientação física */
    *x = (int16_t)((int32_t)ry * 10 / GYRO_SENS_DIV);
    *y = (int16_t)((int32_t)rx * 10 / GYRO_SENS_DIV);
    *z = (int16_t)((int32_t)rz * 10 / GYRO_SENS_DIV);
    return ESP_OK;
}
