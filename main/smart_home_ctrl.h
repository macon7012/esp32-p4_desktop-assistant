#ifndef __SMART_HOME_CTRL_H__
#define __SMART_HOME_CTRL_H__

#include <stdbool.h>
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define LIGHT_LEDC_MODE         LEDC_LOW_SPEED_MODE
#define LIGHT_LEDC_TIMER        LEDC_TIMER_0
#define LIGHT_LEDC_CHANNEL      LEDC_CHANNEL_1
#define LIGHT_GPIO              GPIO_NUM_21
#define LIGHT_LEDC_FREQ         5000
#define LIGHT_LEDC_RES          LEDC_TIMER_10_BIT
#define LIGHT_LEDC_MAX_DUTY     1023

#define FAN_LEDC_MODE           LEDC_LOW_SPEED_MODE
#define FAN_LEDC_TIMER          LEDC_TIMER_0
#define FAN_LEDC_CHANNEL        LEDC_CHANNEL_3
#define FAN_GPIO                GPIO_NUM_7
#define FAN_LEDC_MAX_DUTY       1023
#define FAN_AIN1_GPIO           GPIO_NUM_8
#define FAN_AIN2_GPIO           GPIO_NUM_9

#define FAN_SPEED_OFF           0
#define FAN_SPEED_LOW           1
#define FAN_SPEED_MID           2
#define FAN_SPEED_HIGH          3

#define FAN_DUTY_LOW            65
#define FAN_DUTY_MID            85
#define FAN_DUTY_HIGH           100

typedef struct {
    int light_brightness;
    bool light_on;
    int fan_speed;
    bool fan_on;
    float temperature;
    float humidity;
    bool has_person;
    bool auto_mode;
    bool ai_light_decision;
} smart_home_state_t;

extern smart_home_state_t g_sh_state;

esp_err_t smart_home_ctrl_init(void);
void smart_home_ctrl_set_light(int brightness);
int smart_home_ctrl_get_light(void);
void smart_home_ctrl_light_on(void);
void smart_home_ctrl_light_off(void);
void smart_home_ctrl_light_brighter(void);
void smart_home_ctrl_light_dimmer(void);

void smart_home_ctrl_set_fan(int speed);
int smart_home_ctrl_get_fan(void);
void smart_home_ctrl_fan_on(void);
void smart_home_ctrl_fan_off(void);
void smart_home_ctrl_fan_up(void);
void smart_home_ctrl_fan_down(void);

void smart_home_ctrl_update_sensor(float temp, float hum, bool has_person, bool ai_decision);
void smart_home_ctrl_set_auto_mode(bool enable);
bool smart_home_ctrl_get_auto_mode(void);

#endif