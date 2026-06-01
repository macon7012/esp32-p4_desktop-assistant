#ifndef LED_PWM_H
#define LED_PWM_H

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// 亮度等级枚举
typedef enum {
    LED_BRIGHTNESS_DARK = 0,   // 暗 (20%)
    LED_BRIGHTNESS_MIDDLE,     // 中 (50%)
    LED_BRIGHTNESS_BRIGHT      // 亮 (80%)
} led_brightness_t;

/**
 * @brief 初始化 LED PWM
 * @param gpio_num  LED 连接的 GPIO 引脚号
 * @return ESP_OK 成功，其他失败
 */
esp_err_t led_pwm_init(gpio_num_t gpio_num);

/**
 * @brief 设置 LED 亮度等级
 * @param level 亮度等级 (暗/中/亮)
 */
void led_pwm_set_brightness(led_brightness_t level);

/**
 * @brief 直接设置占空比 (0 ~ 8191, 13位分辨率)
 * @param duty 占空比值
 */
void led_pwm_set_duty(uint16_t duty);

#ifdef __cplusplus
}
#endif

#endif