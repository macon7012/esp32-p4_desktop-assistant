#include "bh1750.h"
#include "myiic.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "BH1750";
static i2c_master_dev_handle_t bh1750_dev_handle;

esp_err_t bh1750_init(void)
{
    esp_err_t ret;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x23,
        .scl_speed_hz = IIC_SPEED_CLK,
    };

    ret = i2c_master_bus_add_device(my_bus_handle, &dev_cfg, &bh1750_dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add BH1750 device: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t cmd = PowerOn;
    ret = i2c_master_transmit(bh1750_dev_handle, &cmd, 1, -1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to power on BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    cmd = HResolutionMode;
    ret = i2c_master_transmit(bh1750_dev_handle, &cmd, 1, -1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set mode: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "BH1750 initialized successfully");
    return ESP_OK;
}

esp_err_t bh1750_read(float *light)
{
    if (light == NULL)
    {
        ESP_LOGE(TAG, "Invalid light pointer");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2] = {0};
    esp_err_t ret = i2c_master_receive(bh1750_dev_handle, data, 2, -1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read data: %s", esp_err_to_name(ret));
        return ret;
    }

    *light = (data[0] << 8 | data[1]) / 1.2f;
    return ESP_OK;
}