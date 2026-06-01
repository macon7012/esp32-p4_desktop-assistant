#include "dht11.h"
#include "dht.h"
#include <esp_log.h>
#include <string.h>
#include <driver/gpio.h>

#define DHT_GPIO_PIN GPIO_NUM_4
static const char *TAG = "dht11";
static gpio_num_t dht11_pin = GPIO_NUM_NC;

esp_err_t dht11_init(gpio_num_t pin)
{
    if (pin < GPIO_NUM_0 || pin >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO pin");
        return ESP_ERR_INVALID_ARG;
    }
    
    dht11_pin = pin;
    
    // 配置 GPIO 为输出开漏模式，初始高电平
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // 初始化为高电平
    gpio_set_level(pin, 1);
    
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d", pin);
    return ESP_OK;
}

esp_err_t dht11_read(dht11_data_t *data)
{
    if (dht11_pin == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "DHT11 not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return ESP_ERR_INVALID_ARG;
    }

    float humidity, temperature;
    esp_err_t ret = dht_read_float_data(DHT_TYPE_DHT11, dht11_pin, &humidity, &temperature);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read data from DHT11: %s", esp_err_to_name(ret));
        return ret;
    }

    data->humidity = humidity;
    data->temperature = temperature;

    ESP_LOGD(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", data->temperature, data->humidity);
    
    return ESP_OK;
} 