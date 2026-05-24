#ifndef __HANDLE_H__
#define __HANDLE_H__

#include "lvgl.h"
#include <stdio.h>
#include "app_main_ui.h"
#include "driver/ledc.h"
#include "myes8311.h"
#include "key.h"
#include "led.h"
#include "my_esp_twai.h"
#include "esp_rtc.h"
#include "app_calculator.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_disp.h"
#include "app_calendar.h"
#include "app_pic.h"
#include "app_music.h"
#include "app_video_ui.h"
#include "app_brush.h"
// #include "app_camera.h"
#include "app_2048.h"
#include "app_file.h"
#include "app_wifi.h"
#include "app_usb_otg.h"
#include "app_timer.h"
#include "voice_control.h"

/* 下拉菜单状态枚举 */
typedef enum
{
    PULL_MENU_CLOSED = 0, /* 下拉菜单关闭 */
    PULL_MENU_OPENED = 1, /* 下拉菜单打开 */
} pull_menu_state_t;

extern pull_menu_state_t pull_menu_state;         /* 下拉菜单状态（外部可见） */
extern SemaphoreHandle_t lv_xGuiSemaphore_handle; /* LVGL信号量句柄（外部可见） */
extern bool lv_gesture_disabled;                  /* 手势禁用标志（外部可见），用于替代危险的remove/add回调模式 */

/* 操作亮度与音量函数 */
void lv_menu_interface_event_cb(lv_event_t *event);
void lv_imgbtn_control_event_handler(lv_event_t *event);
void lv_background_data_processing_timer(lv_timer_t *timer);
void lv_scr_event_cb(lv_event_t *event);
void lv_ui_del(SemaphoreHandle_t BinarySemaphore);
void lv_mbox_event_cb(lv_event_t *e);

esp_err_t bsp_display_brightness_set(int brightness_percent);
esp_err_t bsp_display_voice_set(int voice_percent);

// 翻页功能函数
void lv_app_switch_page(uint8_t page);
void lv_app_create_pages(void);

#endif
