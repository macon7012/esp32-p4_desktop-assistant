#include <stdio.h>
#include "motor.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"




void init_motor_mcpwm(void) {
    // 1. 初始化MCPWM外设
    mcpwm_config_t pwm_config = {
        .frequency = 10000,     // PWM频率10kHz
        .cmpr_a = 0,            // 初始占空比为0
        .cmpr_b = 0,
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode = MCPWM_DUTY_MODE_0,
    };
    mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_1, &pwm_config);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM1A, GPIO_PWM0A);
    
    // 2. 初始化方向控制GPIO
    gpio_set_direction(GPIO_AIN1, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_AIN2, GPIO_MODE_OUTPUT);


}

void set_motor_direction(int speed) {
    if (speed > 0) {  // 正转
        gpio_set_level(GPIO_AIN1, 1);
        gpio_set_level(GPIO_AIN2, 0);
        mcpwm_set_duty(MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_OPR_A, speed);
    } else if (speed < 0) { // 反转
        gpio_set_level(GPIO_AIN1, 0);
        gpio_set_level(GPIO_AIN2, 1);
        mcpwm_set_duty(MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_OPR_A, -speed);
    } else { // 停止
        gpio_set_level(GPIO_AIN1, 0);
        gpio_set_level(GPIO_AIN2, 0);
        mcpwm_set_duty(MCPWM_UNIT_1, MCPWM_TIMER_1, MCPWM_OPR_A, 0);
    }
}

void motor_set_speed_low(void) {
    set_motor_direction(50);  
}

void motor_set_speed_medium(void) {
    set_motor_direction(80);   
}

void motor_set_speed_high(void) {
    set_motor_direction(99);  
}

// 新增：电机停止函数
void motor_stop(void) {
    set_motor_direction(0);
}