#include "i2c_bus.h"
#include "hal_init.h"
#include "esp_log.h"

#define TAG "i2c_bus"

static i2c_master_bus_handle_t s_handle;
static bool                    s_ready = false;

esp_err_t i2c_bus_init(void)
{
    if (s_ready) return ESP_OK;

    i2c_master_bus_config_t cfg = {
        .i2c_port             = HAL_I2C_PORT,
        .sda_io_num           = HAL_I2C_SDA,
        .scl_io_num           = HAL_I2C_SCL,
        .clk_source           = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt    = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&cfg, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
        return err;
    }

    s_ready = true;
    ESP_LOGI(TAG, "I2C_NUM_%d init ok  SDA=%d SCL=%d freq=%dHz",
             (int)HAL_I2C_PORT, HAL_I2C_SDA, HAL_I2C_SCL, HAL_I2C_FREQ_HZ);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_get_handle(void)
{
    return s_handle;
}

esp_err_t i2c_bus_add_device(uint16_t addr_7bit,
                              i2c_master_dev_handle_t *out_dev)
{
    if (!s_ready) {
        ESP_LOGE(TAG, "add_device: bus não inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr_7bit,
        .scl_speed_hz    = HAL_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_master_bus_add_device(s_handle, &dev_cfg, out_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_device 0x%02X: %s", addr_7bit, esp_err_to_name(err));
    }
    return err;
}
