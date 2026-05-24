/**
 ******************************************************************************
 * @file        voice_control.h
 * @version     V1.0
 * @brief       语音控制模块 - 基于ES8311/I2S驱动的语音采集与识别框架
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 * 依赖:        BSP/MYI2S (I2S驱动), BSP/MYES8311 (ES8311编解码器)
 * 核心分配:     采集任务 -> Core 0, 结果处理 -> Core 1 (通过队列通知LVGL)
 ******************************************************************************
 */

#ifndef __VOICE_CONTROL_H__
#define __VOICE_CONTROL_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"

/* 语音指令枚举 - 与智能家居控制功能对应 */
typedef enum
{
    VOICE_CMD_NONE = 0,        /* 无指令 */
    VOICE_CMD_FAN_ON,          /* 风扇: 开 */
    VOICE_CMD_FAN_OFF,         /* 风扇: 关 */
    VOICE_CMD_FAN_LOW,         /* 风扇: 低档 */
    VOICE_CMD_FAN_MID,         /* 风扇: 中档 */
    VOICE_CMD_FAN_HIGH,        /* 风扇: 高档 */
    VOICE_CMD_LIGHT_ON,        /* 灯光: 开 */
    VOICE_CMD_LIGHT_OFF,       /* 灯光: 关 */
    VOICE_CMD_BRIGHTNESS_UP,   /* 亮度: 增加 */
    VOICE_CMD_BRIGHTNESS_DOWN, /* 亮度: 减少 */
    VOICE_CMD_AUTO_ON,         /* 自动模式: 开 */
    VOICE_CMD_AUTO_OFF,        /* 自动模式: 关 */
    VOICE_CMD_CHAT,            /* 对话交互 */
    VOICE_CMD_WAKEUP_START,     /* 唤醒词检测到(UI通知) */
    VOICE_CMD_WAKEUP_END,       /* 唤醒超时结束(UI通知) */
    VOICE_CMD_MAX              /* 指令总数(用于数组大小) */
} voice_cmd_t;

/* 语音指令回调函数类型 - 当识别到语音指令时调用, 在LVGL任务上下文(Core 1)中执行 */
typedef void (*voice_command_callback_t)(voice_cmd_t cmd, int param);

/* 语音控制采集配置 */
typedef struct
{
    uint32_t sample_rate;   /* 采样率(Hz), 默认16000 */
    uint16_t frame_ms;      /* 每帧时长(ms), 默认30 */
    uint16_t recv_buf_size; /* DMA接收缓冲区大小, 默认2400 */
    uint8_t  task_priority; /* 采集任务优先级 */
    uint16_t task_stack;    /* 采集任务栈大小 */
    const char *model_path; /* ESP-SR模型路径, 默认"srmodel" */
    uint16_t mn_timeout_ms; /* 命令词超时(ms), 默认10000 */
    uint16_t vad_min_noise_ms; /* VAD静音检测时长(ms), 默认1500 */
} voice_ctrl_cfg_t;

/* 语音控制模块默认配置 */
#define VOICE_CTRL_INIT_CONFIG() \
    {                            \
        .sample_rate = 16000,    \
        .frame_ms = 30,          \
        .recv_buf_size = 2400,   \
        .task_priority = 6,      \
        .task_stack = 4096,      \
        .model_path = "model", \
        .mn_timeout_ms = 10000,  \
        .vad_min_noise_ms = 1500,\
    }

/* 函数声明 */
esp_err_t voice_control_init(voice_ctrl_cfg_t *cfg);                    /* 初始化语音控制模块 */
esp_err_t voice_control_deinit(void);                                   /* 反初始化语音控制模块 */
esp_err_t voice_control_register_callback(voice_command_callback_t cb); /* 注册语音指令回调函数 */
esp_err_t voice_control_start_listening(void);                          /* 开始采集并识别 */
esp_err_t voice_control_stop_listening(void);                           /* 停止采集识别 */
bool voice_control_is_running(void);                                    /* 查询是否正在采集 */
int voice_control_get_audio_level(void);                                /* 获取当前音频电平(音量) */
void voice_control_process_queue(void);                                 /* 轮询处理语音指令队列(Core 1 LVGL上下文调用) */

#endif /* __VOICE_CONTROL_H__ */