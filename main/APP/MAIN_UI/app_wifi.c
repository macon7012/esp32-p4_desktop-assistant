/**
 ******************************************************************************
 * @file        app_wifi.c
 * @version     V2.0
 * @brief       Wifi APP (重写版 - 极简稳定)
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 
 * 功能: 扫描WiFi列表 + 密码连接 + 上电自动联网
 ******************************************************************************
 */

#include "app_wifi.h"
#include "app_detect.h" /* 添加 CSI 头文件 */
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include <time.h>
#include <string.h>
#include "esp_sntp.h"
#include "esp_rtc.h"

static const char *WIFI_TAG = "WIFI_APP";
wifi_ui_t wifi_ui;

/**************************** 全局变量 ******************************/
static volatile bool wifi_ui_active = false;
static bool wifi_initialized = false;
static bool event_handlers_registered = false;
static bool ntp_started = false;
static bool wifi_was_running = false; /* WiFi是否在运行（用于SD卡插入时保存状态） */
static char connected_ssid[33] = "";

#define MAX_DISPLAY_AP 8
static EventGroupHandle_t wifi_event_group;

/**************************** 函数声明 *****************************/
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ntp_sync_cb(struct timeval *tv);
static void wifi_scan_handler(void);
static void wifi_scan_handler_async_cb(void *arg);
static void scan_btn_event(lv_event_t *e);
static void back_button_event(lv_event_t *e);
static void ap_item_click_event(lv_event_t *e);
static void password_confirm_event(lv_event_t *e);
static void cancel_dialog_event(lv_event_t *e);
static void update_status(const char *text);
static void update_status_cb(void *arg);
static void update_connected_label(void);
static void update_connected_label_cb(void *arg);
void wifi_module_init(void);

/**************************** NTP回调 ******************************/
static void ntp_sync_cb(struct timeval *tv)
{
    struct tm timeinfo;
    localtime_r(&tv->tv_sec, &timeinfo);
    rtc_set_time(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    ESP_LOGI(WIFI_TAG, "NTP synced: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

/**************************** 上电自动连接 ******************************/
void wifi_auto_connect(void)
{
    if (wifi_initialized)
        return;

    ESP_LOGI(WIFI_TAG, "Auto-connect starting...");

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_event_group = xEventGroupCreate();
    wifi_initialized = true;
    event_handlers_registered = true;

    esp_wifi_connect();
    ESP_LOGI(WIFI_TAG, "Auto-connect started");

    /* 初始化 CSI 人体检测模块（2秒检测一次，低资源占用） */
    csi_config_t csi_cfg = {
        .sample_count = CSI_DEFAULT_SAMPLE_COUNT,
        .detection_interval_ms = CSI_DEFAULT_DETECTION_INTERVAL, /* 2秒 */
        .threshold = CSI_DEFAULT_THRESHOLD};
    esp_err_t csi_ret = csi_init(&csi_cfg);
    if (csi_ret == ESP_OK)
    {
        ESP_LOGI(WIFI_TAG, "CSI module initialized, starting continuous detection (2s interval)");
        csi_start_continuous_detection();
    }
    else
    {
        ESP_LOGE(WIFI_TAG, "Failed to initialize CSI module: %s", esp_err_to_name(csi_ret));
    }
}

/**************************** WiFi扫描处理 ******************************/
static void wifi_scan_handler(void)
{
    uint16_t ap_count = 0;
    uint16_t number = MAX_DISPLAY_AP;
    wifi_ap_record_t *ap_info = NULL;

    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(WIFI_TAG, "Scan done: %d APs found, showing max %d", ap_count, number);

    if (ap_count == 0)
    {
        update_status("No networks found");
        return;
    }

    number = (ap_count > number) ? number : ap_count;
    ap_info = calloc(number, sizeof(wifi_ap_record_t));
    if (!ap_info)
    {
        update_status("Memory error");
        return;
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    /* 按信号强度排序 */
    for (int i = 0; i < number - 1; i++)
        for (int j = i + 1; j < number; j++)
            if (ap_info[j].rssi > ap_info[i].rssi)
            {
                wifi_ap_record_t tmp = ap_info[i];
                ap_info[i] = ap_info[j];
                ap_info[j] = tmp;
            }

    /* 删除旧列表并重建（避免lv_obj_clean在flex容器上的问题） */
    if (wifi_ui.list && lv_obj_is_valid(wifi_ui.list))
        lv_obj_del(wifi_ui.list);

    wifi_ui.list = lv_obj_create(wifi_ui.wifi_main_ui);
    lv_obj_set_size(wifi_ui.list, lv_pct(90), lv_pct(55));
    lv_obj_align(wifi_ui.list, LV_ALIGN_TOP_MID, 0, lv_pct(25));
    lv_obj_set_style_bg_color(wifi_ui.list, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_border_width(wifi_ui.list, 0, 0);
    lv_obj_set_style_pad_all(wifi_ui.list, 5, 0);
    lv_obj_set_flex_flow(wifi_ui.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(wifi_ui.list, LV_SCROLLBAR_MODE_ON);

    int shown = 0;
    for (int i = 0; i < number; i++)
    {
        if (strlen((const char *)ap_info[i].ssid) == 0)
            continue;

        /* 每2个AP让出CPU，防止Watchdog */
        if (i > 0 && i % 2 == 0)
            vTaskDelay(pdMS_TO_TICKS(5));

        /* 极简设计：每项仅1个标签，显示SSID + 信号 + 加密方式 */
        char buf[80];
        const char *auth;
        switch (ap_info[i].authmode)
        {
        case WIFI_AUTH_OPEN:
            auth = "[Open]";
            break;
        case WIFI_AUTH_WEP:
            auth = "[WEP]";
            break;
        case WIFI_AUTH_WPA_PSK:
            auth = "[WPA]";
            break;
        case WIFI_AUTH_WPA2_PSK:
            auth = "[WPA2]";
            break;
        case WIFI_AUTH_WPA3_PSK:
            auth = "[WPA3]";
            break;
        default:
            auth = "[*]";
            break;
        }
        snprintf(buf, sizeof(buf), "%-24s  %4ddBm %s",
                 ap_info[i].ssid, ap_info[i].rssi, auth);

        lv_obj_t *item = lv_label_create(wifi_ui.list);
        lv_label_set_text(item, buf);
        lv_obj_set_style_text_font(item, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(item, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_pad_ver(item, 8, 0);
        lv_obj_set_style_pad_hor(item, 10, 0);
        lv_obj_set_size(item, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);

        /* 存储SSID到user_data（使用静态缓冲区避免malloc） */
        char *ssid_copy = strdup((const char *)ap_info[i].ssid);
        lv_obj_add_event_cb(item, ap_item_click_event, LV_EVENT_CLICKED, ssid_copy);
        lv_obj_add_event_cb(item, (lv_event_cb_t)free, LV_EVENT_DELETE, ssid_copy);

        shown++;
    }

    free(ap_info);
    update_status(shown > 0 ? "" : "No visible networks");
    ESP_LOGI(WIFI_TAG, "Displayed %d APs", shown);
}

static void wifi_scan_handler_async_cb(void *arg)
{
    if (!wifi_ui_active)
        return;
    wifi_scan_handler();
}

/**************************** 取消对话框回调 ******************************/
static void cancel_dialog_event(lv_event_t *e)
{
    if (wifi_ui.password_dialog && lv_obj_is_valid(wifi_ui.password_dialog))
        lv_obj_del(wifi_ui.password_dialog);
    wifi_ui.password_dialog = NULL;
    wifi_ui.password_input = NULL;
}

/**************************** AP列表项点击 ******************************/
static void ap_item_click_event(lv_event_t *e)
{
    const char *ssid = (const char *)lv_event_get_user_data(e);
    if (!ssid)
        return;

    ESP_LOGI(WIFI_TAG, "Clicked: %s", ssid);

    /* 保存SSID到结构体，避免悬空指针 */
    strlcpy(wifi_ui.pending_ssid, ssid, sizeof(wifi_ui.pending_ssid));

    /* 弹出密码对话框 */
    if (wifi_ui.password_dialog && lv_obj_is_valid(wifi_ui.password_dialog))
        lv_obj_del(wifi_ui.password_dialog);

    wifi_ui.password_dialog = lv_obj_create(wifi_ui.wifi_main_ui);
    lv_obj_set_size(wifi_ui.password_dialog, 360, 240);
    lv_obj_center(wifi_ui.password_dialog);
    lv_obj_set_style_bg_color(wifi_ui.password_dialog, lv_color_hex(0x2C2C2E), 0);
    lv_obj_set_style_border_width(wifi_ui.password_dialog, 1, 0);
    lv_obj_set_style_border_color(wifi_ui.password_dialog, lv_color_hex(0x48484A), 0);
    lv_obj_set_style_radius(wifi_ui.password_dialog, 12, 0);

    lv_obj_t *title = lv_label_create(wifi_ui.password_dialog);
    lv_label_set_text_fmt(title, "Password for %s", ssid);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    wifi_ui.password_input = lv_textarea_create(wifi_ui.password_dialog);
    lv_textarea_set_one_line(wifi_ui.password_input, true);
    lv_textarea_set_password_mode(wifi_ui.password_input, true);
    lv_textarea_set_placeholder_text(wifi_ui.password_input, "Enter password");
    lv_obj_set_size(wifi_ui.password_input, 300, 45);
    lv_obj_align(wifi_ui.password_input, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(wifi_ui.password_input, lv_color_hex(0x3A3A3C), 0);
    lv_obj_set_style_text_color(wifi_ui.password_input, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(wifi_ui.password_input, lv_color_hex(0x48484A), 0);

    /* 创建键盘并绑定到输入框 */
    lv_obj_t *kb = lv_keyboard_create(wifi_ui.password_dialog);
    lv_keyboard_set_textarea(kb, wifi_ui.password_input);
    lv_obj_set_size(kb, 340, 120);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -55);

    /* Connect按钮 */
    lv_obj_t *btn = lv_btn_create(wifi_ui.password_dialog);
    lv_obj_set_size(btn, 100, 35);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, password_confirm_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Connect");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn_label);

    /* Cancel按钮 */
    lv_obj_t *cancel = lv_btn_create(wifi_ui.password_dialog);
    lv_obj_set_size(cancel, 100, 35);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 20, -10);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_radius(cancel, 8, 0);
    lv_obj_add_event_cb(cancel, cancel_dialog_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(cancel);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_color(cancel_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(cancel_label);
}

/**************************** 密码确认 ******************************/
static void password_confirm_event(lv_event_t *e)
{
    const char *pwd = lv_textarea_get_text(wifi_ui.password_input);
    if (!pwd || strlen(pwd) < 8)
    {
        update_status("Password too short (>=8)");
        return;
    }

    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, wifi_ui.pending_ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, pwd, sizeof(cfg.sta.password));

    ESP_LOGI(WIFI_TAG, "Connecting to %s with password", wifi_ui.pending_ssid);
    update_status("Connecting...");

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();

    if (wifi_ui.password_dialog && lv_obj_is_valid(wifi_ui.password_dialog))
        lv_obj_del(wifi_ui.password_dialog);
    wifi_ui.password_dialog = NULL;
    wifi_ui.password_input = NULL;
}

/**************************** 扫描按钮 ******************************/
static void scan_btn_event(lv_event_t *e)
{
    if (!wifi_initialized)
    {
        wifi_module_init();
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        wifi_initialized = true;
    }

    /* 清除旧列表 */
    if (wifi_ui.list && lv_obj_is_valid(wifi_ui.list))
        lv_obj_del(wifi_ui.list);
    wifi_ui.list = NULL;

    update_status("Scanning...");

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_err_t ret = esp_wifi_scan_start(NULL, false);
    if (ret != ESP_OK)
    {
        ESP_LOGE(WIFI_TAG, "Scan start failed: %s", esp_err_to_name(ret));
        update_status("Scan failed!");
    }
}

/**************************** 状态更新 ******************************/
static void update_status(const char *text)
{
    if (text && wifi_ui_active)
        lv_async_call(update_status_cb, strdup(text));
}

static void update_status_cb(void *arg)
{
    const char *text = (const char *)arg;
    if (wifi_ui.status_label && lv_obj_is_valid(wifi_ui.status_label))
        lv_label_set_text(wifi_ui.status_label, text);
    free(arg);
}

/**************************** 连接状态标签 ******************************/
static void update_connected_label(void)
{
    if (!wifi_ui.connected_label || !lv_obj_is_valid(wifi_ui.connected_label))
        return;

    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK && strlen((const char *)ap.ssid) > 0)
        lv_label_set_text_fmt(wifi_ui.connected_label, "Connected: %s", ap.ssid);
    else
        lv_label_set_text(wifi_ui.connected_label, "Not connected");
}

static void update_connected_label_cb(void *arg)
{
    update_connected_label();
}

/**************************** WiFi事件处理 ******************************/
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_SCAN_DONE:
            ESP_LOGI(WIFI_TAG, "Scan done");
            if (wifi_ui_active)
                lv_async_call(wifi_scan_handler_async_cb, NULL);
            break;
        case WIFI_EVENT_STA_START:
            ESP_LOGI(WIFI_TAG, "STA started");
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGI(WIFI_TAG, "STA stopped");
            break;
        case WIFI_EVENT_STA_CONNECTED:
        {
            wifi_event_sta_connected_t *ev = (wifi_event_sta_connected_t *)event_data;
            strlcpy(connected_ssid, (const char *)ev->ssid, sizeof(connected_ssid));
            ESP_LOGI(WIFI_TAG, "Connected: %s", connected_ssid);
            if (wifi_ui_active)
            {
                update_status("Connected!");
                lv_async_call(update_connected_label_cb, NULL);
            }
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
            connected_ssid[0] = '\0';
            ESP_LOGI(WIFI_TAG, "Disconnected");
            if (wifi_ui_active)
                lv_async_call(update_connected_label_cb, NULL);
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));

        if (!ntp_started)
        {
            setenv("TZ", "CST-8", 1);
            tzset();
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "ntp.aliyun.com");
            esp_sntp_setservername(1, "pool.ntp.org");
            esp_sntp_set_time_sync_notification_cb(ntp_sync_cb);
            esp_sntp_init();
            ntp_started = true;
            ESP_LOGI(WIFI_TAG, "NTP started (UTC+8)");
        }

        if (wifi_ui_active)
            lv_async_call(update_connected_label_cb, NULL);
    }
}

/**************************** WiFi模块初始化 ******************************/
void wifi_module_init(void)
{
    if (!wifi_initialized)
    {
        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        wifi_initialized = true;
        ESP_LOGI(WIFI_TAG, "Module initialized");
    }

    if (!event_handlers_registered)
    {
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
        event_handlers_registered = true;
    }

    if (wifi_event_group == NULL)
        wifi_event_group = xEventGroupCreate();
}

/**************************** 返回按钮 ******************************/
static void back_button_event(lv_event_t *e)
{
    wifi_app_del();
}

/**************************** WiFi应用界面初始化 ******************************/
void wifi_app_init(void)
{
    /* 如果已创建，直接显示 */
    if (wifi_ui.wifi_main_ui && lv_obj_is_valid(wifi_ui.wifi_main_ui))
    {
        wifi_ui_active = true;
        lv_obj_move_foreground(wifi_ui.wifi_main_ui);
        lv_obj_clear_flag(wifi_ui.wifi_main_ui, LV_OBJ_FLAG_HIDDEN);
        update_connected_label();
        ESP_LOGI(WIFI_TAG, "WiFi modal shown (reused)");
        return;
    }

    wifi_ui_active = true;

    /* 主容器 - 深色背景，作为模态弹窗覆盖在屏幕上 */
    wifi_ui.wifi_main_ui = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(wifi_ui.wifi_main_ui, lv_color_hex(0x000000), 0);
    lv_obj_set_size(wifi_ui.wifi_main_ui, lv_pct(100), lv_pct(100));
    lv_obj_set_style_radius(wifi_ui.wifi_main_ui, 0, 0);
    lv_obj_set_style_border_opa(wifi_ui.wifi_main_ui, LV_OPA_0, 0);
    lv_obj_set_pos(wifi_ui.wifi_main_ui, 0, 0);
    lv_obj_clear_flag(wifi_ui.wifi_main_ui, LV_OBJ_FLAG_SCROLLABLE);

    /* 返回按钮 */
    wifi_ui.back_btn = lv_btn_create(wifi_ui.wifi_main_ui);
    lv_obj_set_size(wifi_ui.back_btn, 60, 40);
    lv_obj_align(wifi_ui.back_btn, LV_ALIGN_TOP_LEFT, 15, 15);
    lv_obj_set_style_bg_color(wifi_ui.back_btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(wifi_ui.back_btn, 8, 0);
    lv_obj_add_event_cb(wifi_ui.back_btn, back_button_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(wifi_ui.back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(back_label);

    /* 标题 */
    lv_obj_t *title = lv_label_create(wifi_ui.wifi_main_ui);
    lv_label_set_text(title, "WiFi");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* 扫描按钮 */
    wifi_ui.scan_btn = lv_btn_create(wifi_ui.wifi_main_ui);
    lv_obj_set_size(wifi_ui.scan_btn, 80, 40);
    lv_obj_align(wifi_ui.scan_btn, LV_ALIGN_TOP_RIGHT, -15, 15);
    lv_obj_set_style_bg_color(wifi_ui.scan_btn, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_radius(wifi_ui.scan_btn, 8, 0);
    lv_obj_add_event_cb(wifi_ui.scan_btn, scan_btn_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *scan_label = lv_label_create(wifi_ui.scan_btn);
    lv_label_set_text(scan_label, "Scan");
    lv_obj_set_style_text_color(scan_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(scan_label, &lv_font_montserrat_16, 0);
    lv_obj_center(scan_label);

    /* 连接状态 */
    wifi_ui.connected_label = lv_label_create(wifi_ui.wifi_main_ui);
    lv_label_set_text(wifi_ui.connected_label, "Not connected");
    lv_obj_set_style_text_font(wifi_ui.connected_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_ui.connected_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(wifi_ui.connected_label, LV_ALIGN_TOP_MID, 0, 55);

    /* 状态标签 */
    wifi_ui.status_label = lv_label_create(wifi_ui.wifi_main_ui);
    lv_label_set_text(wifi_ui.status_label, "");
    lv_obj_set_style_text_font(wifi_ui.status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_ui.status_label, lv_color_hex(0x34C759), 0);
    lv_obj_align(wifi_ui.status_label, LV_ALIGN_TOP_MID, 0, 75);

    /* 初始化WiFi模块 */
    wifi_module_init();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    wifi_initialized = true;
    update_connected_label();

    ESP_LOGI(WIFI_TAG, "WiFi app initialized");
}

/**************************** WiFi应用清理 ******************************/
void wifi_app_del(void)
{
    wifi_ui_active = false;

    /* 关闭密码对话框 */
    if (wifi_ui.password_dialog && lv_obj_is_valid(wifi_ui.password_dialog))
    {
        lv_obj_del(wifi_ui.password_dialog);
        wifi_ui.password_dialog = NULL;
        wifi_ui.password_input = NULL;
    }

    /* 隐藏前先移到背景，彻底避免z-order干扰 */
    if (wifi_ui.wifi_main_ui && lv_obj_is_valid(wifi_ui.wifi_main_ui))
    {
        lv_obj_move_background(wifi_ui.wifi_main_ui);
        lv_obj_add_flag(wifi_ui.wifi_main_ui, LV_OBJ_FLAG_HIDDEN);
    }

    ESP_LOGI(WIFI_TAG, "WiFi modal hidden");
}

/**************************** WiFi暂停（SD卡插入时调用） ******************************/
void wifi_suspend(void)
{
    if (!wifi_initialized)
        return;

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL)
    {
        ESP_LOGI(WIFI_TAG, "Suspending WiFi due to SD card insertion...");
        esp_wifi_stop();
        wifi_was_running = true;
    }
}

/**************************** WiFi恢复（SD卡拔出时调用） ******************************/
void wifi_resume(void)
{
    if (!wifi_initialized || !wifi_was_running)
        return;

    ESP_LOGI(WIFI_TAG, "Resuming WiFi after SD card removal...");
    esp_wifi_start();
    esp_wifi_connect();
    wifi_was_running = false;
}

/**************************** WiFi重试（SD卡拔出后10秒内重试） ******************************/
void wifi_retry_if_needed(void)
{
    if (!wifi_initialized)
        return;

    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret == ESP_OK && mode == WIFI_MODE_NULL)
    {
        ESP_LOGI(WIFI_TAG, "WiFi is off, attempting to restart...");
        esp_wifi_start();
        esp_wifi_connect();
    }
}