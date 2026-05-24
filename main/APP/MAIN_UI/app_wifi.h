/**
 ******************************************************************************
 * @file        app_wifi.h
 * @version     V1.0
 * @brief       Wifi APP
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#ifndef _APP_WIFI_H
#define _APP_WIFI_H

#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "app_main_ui.h"
#include "lvgl_demo.h"
#include "lcd.h"

/* 结构体声明 */
typedef struct
{
    lv_obj_t *wifi_main_ui;
    lv_obj_t *list;
    lv_obj_t *scan_btn;
    lv_obj_t *back_btn;
    lv_obj_t *status_label;
    lv_obj_t *connected_label;
    lv_obj_t *password_dialog;
    lv_obj_t *password_input;
    char pending_ssid[33];  /* 待连接SSID副本，避免悬空指针 */
    uint8_t initialized;
} wifi_ui_t;

extern wifi_ui_t wifi_ui;

/* 函数声明 */
void wifi_app_init(void);
void wifi_app_del(void);
void wifi_auto_connect(void);  /* 上电自动连接WiFi */
void wifi_suspend(void);       /* 暂停WiFi（用于SD卡插入时） */
void wifi_resume(void);        /* 恢复WiFi（用于SD卡拔出时） */
void wifi_retry_if_needed(void); /* WiFi重试（拔出SD卡后10秒内重试） */

#endif