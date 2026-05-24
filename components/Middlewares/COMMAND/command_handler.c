#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "command_handler.h"

// ========== 硬件配置 ==========
#define LED_PIN         GPIO_NUM_13  // 控制 GPIO13 的 LED

// ========== 命令ID定义 (前九条指令) ==========
typedef enum {
    CMD_BA_XIAO_SHI_HOU_GUAN_JI = 1,   // "ba xiao shi hou guan ji" - 八小时后关机
    CMD_BA_XIAO_SHI_HOU_KAI_JI  = 2,   // "ba xiao shi hou kai ji"  - 八小时后开机
    CMD_BI_KAI_WO_CHUI          = 3,   // "bi kai wo chui"          - 避开我吹
    CMD_CHAO_QIANG_FENG_SU      = 4,   // "chao qiang feng su"      - 超强风速
    CMD_CHAO_ZHE_WO_CHUI        = 5,   // "chao zhe wo chui"        - 朝着我吹
    CMD_CHOU_SHI_MO_SHI         = 6,   // "chou shi mo shi"         - 除湿模式
    CMD_CHU_SHI_MO_SHI          = 7,   // "chu shi mo shi"          - 出湿模式
    CMD_DA_KAI_BAI_FENG         = 8,   // "da kai bai feng"         - 打开摆风
    CMD_DA_KAI_BING_XIANG       = 9,   // "da kai bing xiang"       - 打开冰箱
} command_id_t;

// ========== 私有函数声明 ==========
static void gpio_init(void);
static void led_control(bool on);

// ========== 实现 ==========

esp_err_t command_handler_init(void) {
    gpio_init();
    printf("Command handler initialized\n");
    return ESP_OK;
}

void command_handler_execute(int command_id) {
    printf("[CMD] Executing command ID: %d\n", command_id);
    
    switch (command_id) {
        case CMD_BI_KAI_WO_CHUI:          // 指令3: 避开我吹 -> 关闭 LED
            printf("[CMD] 指令3: 避开我吹 -> 关闭 LED\n");
            led_control(false);
            break;
            
        case CMD_CHAO_QIANG_FENG_SU:      // 指令4: 超强风速 -> 打开 LED
            printf("[CMD] 指令4: 超强风速 -> 打开 LED\n");
            led_control(true);
            break;
            
        // 其他指令暂时不处理
        case CMD_BA_XIAO_SHI_HOU_GUAN_JI:
        case CMD_BA_XIAO_SHI_HOU_KAI_JI:
        case CMD_CHAO_ZHE_WO_CHUI:
        case CMD_CHOU_SHI_MO_SHI:
        case CMD_CHU_SHI_MO_SHI:
        case CMD_DA_KAI_BAI_FENG:
        case CMD_DA_KAI_BING_XIANG:
            printf("[CMD] 指令%d: 暂未实现\n", command_id);
            break;
            
        default:
            printf("[CMD] Unknown command ID: %d\n", command_id);
    }
}

void command_handler_deinit(void) {
    gpio_set_level(LED_PIN, 0);
    printf("Command handler deinitialized\n");
}

// ========== 硬件控制函数 ==========

static void gpio_init(void) {
    // 配置 LED 引脚 (GPIO13)
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_PIN, 0);  // 默认关闭
}

static void led_control(bool on) {
    gpio_set_level(LED_PIN, on ? 1 : 0);
    printf("[LED] GPIO13 %s\n", on ? "ON" : "OFF");
}
