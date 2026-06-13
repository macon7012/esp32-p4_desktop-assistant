#include "smart_home_ctrl.h"
#include "esp_log.h"

static const char *SHC_TAG = "smart_home";

smart_home_state_t g_sh_state = {
    .light_brightness = 0,
    .light_on = false,
    .fan_speed = FAN_SPEED_OFF,
    .fan_on = false,
    .temperature = 0,
    .humidity = 0,
    .has_person = false,
    .auto_mode = false,
    .ai_light_decision = false,
};

esp_err_t smart_home_ctrl_init(void)
{
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LIGHT_LEDC_MODE,
        .channel = LIGHT_LEDC_CHANNEL,
        .timer_sel = LIGHT_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LIGHT_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_LOGI(SHC_TAG, "Light PWM initialized on GPIO%d", (int)LIGHT_GPIO);

    ledc_channel_config_t fan_channel = {
        .speed_mode = FAN_LEDC_MODE,
        .channel = FAN_LEDC_CHANNEL,
        .timer_sel = FAN_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = FAN_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&fan_channel));

    gpio_set_direction(FAN_AIN1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(FAN_AIN2_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_level(FAN_AIN1_GPIO, 0);
    gpio_set_level(FAN_AIN2_GPIO, 0);

    ESP_LOGI(SHC_TAG, "Fan PWM initialized on GPIO%d", (int)FAN_GPIO);

    return ESP_OK;
}

static void _apply_light_duty(int brightness)
{
    uint32_t duty = (LIGHT_LEDC_MAX_DUTY * brightness) / 100;
    ledc_set_duty(LIGHT_LEDC_MODE, LIGHT_LEDC_CHANNEL, duty);
    ledc_update_duty(LIGHT_LEDC_MODE, LIGHT_LEDC_CHANNEL);
}

void smart_home_ctrl_set_light(int brightness)
{
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;

    g_sh_state.light_brightness = brightness;
    g_sh_state.light_on = (brightness > 0);

    _apply_light_duty(brightness);

    ESP_LOGI(SHC_TAG, "Light set: %d%%", brightness);
}

int smart_home_ctrl_get_light(void)
{
    return g_sh_state.light_brightness;
}

void smart_home_ctrl_light_on(void)
{
    smart_home_ctrl_set_light(100);
}

void smart_home_ctrl_light_off(void)
{
    smart_home_ctrl_set_light(0);
}

void smart_home_ctrl_light_brighter(void)
{
    int b = g_sh_state.light_brightness + 10;
    if (b > 100) b = 100;
    smart_home_ctrl_set_light(b);
}

void smart_home_ctrl_light_dimmer(void)
{
    int b = g_sh_state.light_brightness - 10;
    if (b < 0) b = 0;
    smart_home_ctrl_set_light(b);
}

static void _apply_fan_speed(int speed)
{
    uint32_t duty = 0;
    switch (speed) {
        case FAN_SPEED_LOW:  duty = (FAN_LEDC_MAX_DUTY * FAN_DUTY_LOW)  / 100; break;
        case FAN_SPEED_MID:  duty = (FAN_LEDC_MAX_DUTY * FAN_DUTY_MID)  / 100; break;
        case FAN_SPEED_HIGH: duty = (FAN_LEDC_MAX_DUTY * FAN_DUTY_HIGH) / 100; break;
        default:             duty = 0;                                           break;
    }

    if (duty > 0) {
        gpio_set_level(FAN_AIN1_GPIO, 1);
        gpio_set_level(FAN_AIN2_GPIO, 0);
    } else {
        gpio_set_level(FAN_AIN1_GPIO, 0);
        gpio_set_level(FAN_AIN2_GPIO, 0);
    }
    ledc_set_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL, duty);
    ledc_update_duty(FAN_LEDC_MODE, FAN_LEDC_CHANNEL);
}

void smart_home_ctrl_set_fan(int speed)
{
    if (speed < FAN_SPEED_OFF) speed = FAN_SPEED_OFF;
    if (speed > FAN_SPEED_HIGH) speed = FAN_SPEED_HIGH;

    g_sh_state.fan_speed = speed;
    g_sh_state.fan_on = (speed > FAN_SPEED_OFF);

    _apply_fan_speed(speed);

    ESP_LOGI(SHC_TAG, "Fan set: %s (speed=%d)", g_sh_state.fan_on ? "ON" : "OFF", speed);
}

int smart_home_ctrl_get_fan(void)
{
    return g_sh_state.fan_speed;
}

void smart_home_ctrl_fan_on(void)
{
    smart_home_ctrl_set_fan(FAN_SPEED_LOW);
}

void smart_home_ctrl_fan_off(void)
{
    smart_home_ctrl_set_fan(FAN_SPEED_OFF);
}

void smart_home_ctrl_fan_up(void)
{
    int s = g_sh_state.fan_speed;
    if (s < FAN_SPEED_HIGH) {
        smart_home_ctrl_set_fan(s + 1);
    }
}

void smart_home_ctrl_fan_down(void)
{
    int s = g_sh_state.fan_speed;
    if (s > FAN_SPEED_OFF) {
        smart_home_ctrl_set_fan(s - 1);
    }
}

void smart_home_ctrl_update_sensor(float temp, float hum, bool has_person, bool ai_decision)
{
    g_sh_state.temperature = temp;
    g_sh_state.humidity = hum;
    g_sh_state.has_person = has_person;
    g_sh_state.ai_light_decision = ai_decision;
}

void smart_home_ctrl_set_auto_mode(bool enable)
{
    g_sh_state.auto_mode = enable;
    ESP_LOGI(SHC_TAG, "Auto mode: %s", enable ? "ON" : "OFF");
}

bool smart_home_ctrl_get_auto_mode(void)
{
    return g_sh_state.auto_mode;
}