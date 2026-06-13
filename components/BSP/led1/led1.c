#include "led.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "LED_PWM";

// LEDC 配置参数
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_FREQ_HZ    5000
#define LEDC_RESOLUTION LEDC_TIMER_13_BIT   // 0 ~ 8191

// 预定义亮度占空比 (13位分辨率)
#define DUTY_DARK   1638    // 20%
#define DUTY_MIDDLE 4095    // 50%
#define DUTY_BRIGHT 6553    // 80%

static uint16_t duty_table[] = {
    DUTY_DARK,
    DUTY_MIDDLE,
    DUTY_BRIGHT
};

esp_err_t led_pwm_init(gpio_num_t gpio_num)
{
    // 1. 配置定时器
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_RESOLUTION,
        .freq_hz          = LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %d", ret);
        return ret;
    }

    // 2. 配置通道
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .gpio_num       = gpio_num,
        .duty           = 0,
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "LED PWM initialized on GPIO %d", gpio_num);
    return ESP_OK;
}

void led_pwm_set_brightness(led_brightness_t level)
{
    if (level > LED_BRIGHTNESS_BRIGHT) {
        ESP_LOGW(TAG, "Invalid brightness level, use BRIGHT instead");
        level = LED_BRIGHTNESS_BRIGHT;
    }
    uint16_t duty = duty_table[level];
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    ESP_LOGD(TAG, "Set brightness level %d, duty = %d", level, duty);
}

void led_pwm_set_duty(uint16_t duty)
{
    if (duty > 8191) duty = 8191;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}