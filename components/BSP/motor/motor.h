// 引脚定义
#define GPIO_PWM0A  7
#define GPIO_AIN1   8
#define GPIO_AIN2   9


void init_motor_mcpwm(void);
void motor_set_speed_low(void);
void motor_set_speed_medium(void);
void motor_set_speed_high(void);
void motor_stop(void);
