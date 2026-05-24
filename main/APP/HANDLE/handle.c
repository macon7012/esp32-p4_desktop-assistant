#include "handle.h"
#include "app_wifi.h"
#include "app_usb_otg.h"
#include "app_file.h"

static const char *HANDLE_TAG = "HANDLE";

// 定义就绪列表
unsigned int app_readly_list[32]; /* 前导置零 */
/* 菜单图标就绪列表 */
unsigned int menu_readly_list[10]; /* 前导置零 */
// 定义触发标志
uint8_t lv_trigger_bit = 0; /* 触发标志 */
// 定义加载索引
uint8_t load_index = 0; /* 加载索引 */

SemaphoreHandle_t lv_xGuiSemaphore_handle;

static bool is_blue[4] = {false, false, false, false}; /* 默认是灰色 */

// 判断是否在2048应用中
static bool is_in_2048_app = false;

/* 手势已处理标志：同一触摸中若已触发手势，则忽略后续APP按钮点击 */
static bool lv_gesture_processed = false;

/* 手势禁用标志：应用可设置此标志临时禁用屏幕手势回调，替代危险的remove/add回调模式 */
bool lv_gesture_disabled = false;

// 智能家居控制相关全局变量
static lv_obj_t *fan_low_btn = NULL;
static lv_obj_t *fan_mid_btn = NULL;
static lv_obj_t *fan_high_btn = NULL;
static lv_obj_t *fan_status_cont = NULL;
static lv_obj_t *fan_status_label = NULL;
static lv_obj_t *chat_card = NULL;

/**
 * @brief  前导置零
 * @param  app_readly_list:就绪列表
 * @retval 返回APP按下的数值
 */
int lv_clz(unsigned int app_readly_list[])
{
    int bit = 0;

    for (int i = 0; i < 32; i++)
    {
        if (app_readly_list[i] == 1)
        {
            break;
        }

        bit++;
    }

    return bit;
}

/**************************** 用户处理接口 ********************************/
/**
 * @brief  菜单界面处理
 * @param  event : 事件
 * @retval 无
 */
void lv_menu_interface_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (code == LV_EVENT_PRESSING)
    {
        if (obj == lv_app_ui.app_small_ui.small_cont_brightness)
        {
            /* 控制屏幕亮度操作 */
            bsp_display_brightness_set((int)lv_slider_get_value(obj));
        }
        else if (obj == lv_app_ui.app_small_ui.small_cont_voice)
        {
            /* 设置ES8311音量 */
            esp_codec_dev_set_out_vol(codec_handle, (int)lv_slider_get_value(obj));
        }
    }
    else if (code == LV_EVENT_CLICKED)
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            // 判断点击的是否是当前按键
            if (obj == lv_app_ui.app_small_ui.small_cont_flexo[i])
            {
                if (is_blue[i])
                {
                    // 如果当前按键是蓝色，点击后变为灰色
                    lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_flexo[i], lv_color_make(50, 50, 50), LV_STATE_DEFAULT); // 灰色
                }
                else
                {
                    if (obj == lv_app_ui.app_small_ui.small_cont_flexo[2])
                    {
                        // 如果当前按键是灰色，点击后变为绿色
                        lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_flexo[i], lv_color_make(101, 196, 102), LV_STATE_DEFAULT); // 蓝色
                    }
                    else
                    {
                        // 如果当前按键是灰色，点击后变为蓝色
                        lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont_flexo[i], lv_color_make(52, 120, 245), LV_STATE_DEFAULT); // 蓝色
                    }
                }

                // 切换当前按键的颜色状态
                is_blue[i] = !is_blue[i];
            }

            if (is_blue[i] == true && obj == lv_app_ui.app_small_ui.small_cont_flexo[i])
            {
                menu_readly_list[i] = 1;

                lv_trigger_bit = ((unsigned int)lv_clz((menu_readly_list)));
                menu_readly_list[lv_trigger_bit] = 0;
                printf("开启menu_trigger_bit:%d\n", lv_trigger_bit);
                switch (lv_trigger_bit)
                {
                case 0:
                    /* 开启蓝牙操作 */
                    break;

                case 1:
                    /* 开启飞行模式操作 */
                    break;

                case 2:
                    /* 开启移动模式操作 */
                    break;

                case 3:
                    /* 先关闭下拉菜单，再打开WiFi模态弹窗 */
                    lv_anim_h_act(lv_app_ui.app_small_ui.small_cont, lv_obj_get_height(lv_scr_act()) / 20, true);
                    pull_menu_state = PULL_MENU_CLOSED;
                    for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                    {
                        lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
                    }
                    wifi_app_init();
                    break;
                }
            }
            else if (is_blue[i] == false && obj == lv_app_ui.app_small_ui.small_cont_flexo[i])
            {
                menu_readly_list[i] = 1;

                lv_trigger_bit = ((unsigned int)lv_clz((menu_readly_list)));
                menu_readly_list[lv_trigger_bit] = 0;
                printf("关闭menu_trigger_bit:%d\n", lv_trigger_bit);
                switch (lv_trigger_bit)
                {
                case 0:
                    /* 关闭蓝牙操作 */
                    break;

                case 1:
                    /* 关闭飞行模式操作 */
                    break;

                case 2:
                    /* 关闭移动模式操作 */
                    break;

                case 3:

                    wifi_app_del(); // WiFi已禁用
                    for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                    {
                        lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
                    }
                    break;
                }
            }
        }
    }
}

/**
 * @brief  主界面的APP图标处理
 * @param  event : 事件
 * @retval 无
 */
void lv_imgbtn_control_event_handler(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);

    if (code == LV_EVENT_CLICKED)
    {
        /* 如果本次触摸已触发手势，忽略APP按钮点击 */
        if (lv_gesture_processed)
        {
            /* 重置标志，确保下次触摸可以正常点击 */
            lv_gesture_processed = false;
            return;
        }

        for (int i = 0; i < MAIN_APP_NUM; i++)
        {
            if (obj == lv_app_ui.app_main_ui.app_btn[i])
            {
                app_readly_list[i] = 1;
            }
        }

        lv_trigger_bit = ((unsigned int)lv_clz((app_readly_list)));
        app_readly_list[lv_trigger_bit] = 0;
        printf("lv_trigger_bit:%d\n", lv_trigger_bit);
        switch (lv_trigger_bit)
        {
        case 0:
            lv_app_usb_otg_init();
            is_in_2048_app = false;
            break;

        case 1:
            lv_obj_add_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
            for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
            {
                lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
            }
            lv_app_brush_init();
            is_in_2048_app = false;
            break;

        case 2:
            lv_obj_add_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
            for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
            {
                lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
            }
            lv_app_calculator_init();
            is_in_2048_app = false;
            break;

        case 3:
            lv_obj_add_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
            for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
            {
                lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
            }
            app_calendar_ui_init();
            is_in_2048_app = false;
            break;
        case 4:
            lv_obj_add_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
            for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
            {
                lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
            }
            // app_camera_ui_init();
            is_in_2048_app = false;
            break;

        case 5:
            app_file_ui_init();
            lv_general.del_function = app_file_ui_del;
            is_in_2048_app = false;
            break;

        case 6:
            lv_app_music_init();
            is_in_2048_app = false;
            break;

        case 7:
            /* WiFi已移至顶部图标栏，此处保留为后续扩展 */
            break;

        case 8:
            lv_app_video_init();
            is_in_2048_app = false;
            break;
        case 9:
            app_2048_ui_init();
            is_in_2048_app = true;
            break;
        case 10:
            lv_app_timer_init();
            is_in_2048_app = false;
            break;
        case 11:
            lv_app_pic_init();
            is_in_2048_app = false;
            break;
        default:
            break;
        }
    }
}

pull_menu_state_t pull_menu_state = PULL_MENU_CLOSED; /* 下拉菜单状态 */

// 翻页相关变量
#define TOTAL_PAGES 2                    /* 总页数 */
static uint8_t current_page = 0;         /* 当前页码: 0=现有主界面, 1=空白页 */
static lv_obj_t *page2_container = NULL; /* 第二页容器 */

// 智能家居控制事件回调函数声明
static void lv_fan_speed_event(lv_event_t *e);
static void lv_fan_switch_event(lv_event_t *e);
static void lv_light_switch_event(lv_event_t *e);
static void lv_auto_light_switch_event(lv_event_t *e);
static void lv_voice_command_callback(voice_cmd_t cmd, int param);
static void lv_update_presence_status(bool has_person);
static void lv_brightness_slider_event(lv_event_t *e);

/**
 * @brief  屏幕事件回调函数
 * @param  event : 事件
 * @retval 无
 */
void lv_scr_event_cb(lv_event_t *event)
{
    if (lv_gesture_disabled)
    {
        /* 如果当前有子界面正在运行，说明可能是在应用内点击后直接手势退出，
         * 这种情况下应该允许手势退出，强制清除禁用标志
         */
        if (lv_general.current_parent != NULL && lv_event_get_code(event) == LV_EVENT_GESTURE)
        {
            lv_gesture_disabled = false;
        }
        else
        {
            return;
        }
    }

    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *obj = lv_event_get_target(event);
    // 如果事件代码为LV_EVENT_GESTURE
    if (code == LV_EVENT_GESTURE)
    {
        /* WiFi模态弹窗可见时，忽略所有手势 */
        if (wifi_ui.wifi_main_ui && lv_obj_is_valid(wifi_ui.wifi_main_ui) &&
            !lv_obj_has_flag(wifi_ui.wifi_main_ui, LV_OBJ_FLAG_HIDDEN))
        {
            return;
        }

        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        if (dir == LV_DIR_BOTTOM && pull_menu_state == PULL_MENU_CLOSED && lv_touch.lv_touch_cont == NULL)
        {
            if (!is_in_2048_app)
            {
                /* 下拉打开菜单：标记手势已处理，阻止同一次触摸触发APP按钮点击 */
                lv_gesture_processed = true;

                lv_obj_set_style_bg_color(lv_app_ui.app_small_ui.small_cont, lv_color_hex(0x000000), LV_STATE_DEFAULT);
                for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                {
                    lv_obj_clear_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
                }
                lv_pull_ui(lv_app_ui.app_small_ui.small_cont);
                pull_menu_state = PULL_MENU_OPENED;
            }
        }
        else if (dir == LV_DIR_TOP)
        {
            if (pull_menu_state == PULL_MENU_OPENED)
            {
                /* 上滑关闭菜单：标记手势已处理 */
                lv_gesture_processed = true;

                lv_anim_h_act(lv_app_ui.app_small_ui.small_cont, lv_obj_get_height(lv_scr_act()) / 20, true);
                pull_menu_state = PULL_MENU_CLOSED;
                for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                {
                    lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
                }
            }
            else if (lv_general.current_parent != NULL && !is_in_2048_app)
            {
                /* 上滑退出应用：标记手势已处理，阻止同一次触摸触发APP按钮点击 */
                lv_gesture_processed = true;

                /* 摄像头已移除
                if (lv_general.current_parent == lv_camera_ui.camera_main_ui)
                {
                    app_camera_exit();
                    xSemaphoreGive(lv_xGuiSemaphore_handle);
                }
                else
                */
                {
                    lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(lv_general.current_parent, LV_OBJ_FLAG_HIDDEN);
                    xSemaphoreGive(lv_xGuiSemaphore_handle);
                }
            }
        }
        else if (dir == LV_DIR_LEFT)
        {
            if (lv_pic_ui.pic_main_ui != NULL)
            {
                xSemaphoreGive(xSemaphore_next);
            }
            else if (!is_in_2048_app)
            {
                if (pull_menu_state == PULL_MENU_OPENED)
                {
                    /* 左滑关闭菜单：标记手势已处理 */
                    lv_gesture_processed = true;

                    lv_anim_h_act(lv_app_ui.app_small_ui.small_cont, lv_obj_get_height(lv_scr_act()) / 20, true);
                    pull_menu_state = PULL_MENU_CLOSED;
                    for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
                    {
                        lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
                    }
                }
                else if (pull_menu_state == PULL_MENU_CLOSED && current_page == 0 && lv_general.current_parent == NULL)
                {
              
                    current_page = 1;
                    lv_app_switch_page(current_page);
                }
            }
        }
        else if (dir == LV_DIR_RIGHT)
        {
            if (lv_pic_ui.pic_main_ui != NULL)
            {
                xSemaphoreGive(xSemaphore_prev);
            }
            else
            {
                if (current_page == 1)
                {
                    /* 右滑返回主页：不设置手势标志，允许后续点击APP按钮 */
                    current_page = 0;
                    lv_app_switch_page(current_page);
                }
            }
        }

        /* 阻止手势事件继续冒泡，防止被其他handler误处理 */
        lv_event_stop_bubbling(event);
    }
    else if (code == LV_EVENT_PRESSED)
    {
        /* 新触摸开始，清除手势标志 */
        lv_gesture_processed = false;
    }
}

/* 界面退出后删除子界面 */
void lv_ui_del(SemaphoreHandle_t BinarySemaphore)
{
    lv_xGuiSemaphore_handle = BinarySemaphore;
    xSemaphoreTake(BinarySemaphore, portMAX_DELAY); /* 同步删除子界面信号量 */

    /* 锁定互斥锁，因为LVGL API不是线程安全的 */
    bool lock_success = lvgl_port_lock(pdMS_TO_TICKS(5000));
    if (lock_success)
    {
        if (lv_general.current_parent != NULL)
        {
            lv_obj_del(lv_general.current_parent);
        }

        if (lv_general.del_function != NULL)
        {
            lv_general.del_function();
            lv_general.del_function = NULL;
        }
        lv_general.current_parent = NULL;
        lv_obj_update_layout(lv_scr_act());

        /* 当子界面被删除时，重置2048标志 */
        is_in_2048_app = false;

        /* 恢复主界面容器显示 */
        lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);

        /* 直接恢复所有主应用按钮的可点击状态 */
        for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
        {
            lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
        }

        /* 在释放锁之前发送事件，确保线程安全 */
        lv_event_send(lv_scr_act(), LV_EVENT_RELEASED, NULL);

        /* 释放互斥锁 */
        lvgl_port_unlock();
    }
    else
    {
        /* 锁获取失败，强制清理状态并释放信号量，避免永久阻塞 */
        ESP_LOGW(HANDLE_TAG, "lvgl_port_lock timeout, forcing cleanup");
        lv_general.current_parent = NULL;
        lv_general.del_function = NULL;
        is_in_2048_app = false;
        lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
        for (uint8_t index = 0; index < MAIN_APP_NUM; index++)
        {
            lv_obj_add_flag(lv_app_ui.app_main_ui.app_btn[index], LV_OBJ_FLAG_CLICKABLE);
        }
    }

    lv_gesture_processed = false;

    /* 确保手势禁用标志被清除，防止应用异常退出后手势永久失效 */
    lv_gesture_disabled = false;

    vTaskDelay(pdMS_TO_TICKS(50));
}

/**
 * @brief  创建第二页（深蓝色空白页）
 * @param  无
 * @retval 无
 */
void lv_app_create_pages(void)
{
    // 创建第二页容器
    page2_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page2_container, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));

    static lv_style_t bg_style;
    lv_style_init(&bg_style);

    lv_style_set_bg_color(&bg_style, lv_color_hex(0x0F172A));
    // lv_style_set_bg_grad_color(&bg_style, lv_color_hex(0xC8B6E2));
    // lv_style_set_bg_grad_dir(&bg_style, LV_GRAD_DIR_VER);
    lv_obj_add_style(page2_container, &bg_style, 0);

    lv_obj_set_style_border_opa(page2_container, LV_OPA_0, LV_STATE_DEFAULT); /* 去除边框 */
    lv_obj_set_style_radius(page2_container, 0, LV_STATE_DEFAULT);            /* 去除圆角，避免漏光 */
    lv_obj_set_style_bg_opa(page2_container, LV_OPA_100, LV_STATE_DEFAULT);   /* 确保背景完全不透明 */
    lv_obj_clear_flag(page2_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(page2_container, LV_OBJ_FLAG_HIDDEN); // 默认隐藏

    // 为第二页添加手势事件回调，确保能响应右滑手势
    lv_obj_add_event_cb(page2_container, lv_scr_event_cb, LV_EVENT_GESTURE, NULL);

    // ==================== 智能家居控制界面（Flex布局） ====================

    // 设置 page2_container 为 flex 列布局，子元素从上到下排列
    lv_obj_set_flex_flow(page2_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(page2_container, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(page2_container, 10, LV_STATE_DEFAULT);

    // 页面标题
    lv_obj_t *page_title = lv_label_create(page2_container);
    lv_label_set_text(page_title, "Control Center ");
    lv_obj_set_style_text_font(page_title, &lv_font_montserrat_32, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(page_title, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);

    // ==================== 内容行（左：环境+状态，右：风扇+灯光） ====================
    int screen_h = lv_obj_get_height(lv_scr_act());
    lv_obj_t *content_row = lv_obj_create(page2_container);
    lv_obj_set_size(content_row, LV_PCT(100), screen_h * 6 / 10);
    lv_obj_set_style_pad_all(content_row, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(content_row, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(content_row, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(content_row, LV_OBJ_FLAG_SCROLLABLE);

    // ---- 左列容器 ----
    lv_obj_t *left_col = lv_obj_create(content_row);
    lv_obj_set_size(left_col, LV_PCT(50), LV_PCT(100));
    lv_obj_set_style_pad_all(left_col, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(left_col, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(left_col, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(left_col, LV_OBJ_FLAG_SCROLLABLE);

    // -------------------- 1. 环境信息卡片 --------------------
    lv_obj_t *env_card = lv_obj_create(left_col);
    lv_obj_set_size(env_card, LV_PCT(100), LV_PCT(55));
    lv_obj_set_style_bg_color(env_card, lv_color_hex(0x1E293B), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(env_card, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(env_card, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(env_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(env_card, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *env_title = lv_label_create(env_card);
    lv_label_set_text(env_title, "Environment");
    lv_obj_set_style_text_font(env_title, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(env_title, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_align(env_title, LV_ALIGN_TOP_LEFT, 20, 15);

    // 温湿度显示区域 - 左对齐 Environment 文本
    lv_obj_t *temp_humi_cont = lv_obj_create(env_card);
    lv_obj_set_size(temp_humi_cont, LV_PCT(85), 90);
    lv_obj_set_flex_flow(temp_humi_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(temp_humi_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(temp_humi_cont, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(temp_humi_cont, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_align(temp_humi_cont, LV_ALIGN_TOP_LEFT, 20, 48);

    // 温度图标+数值
    lv_obj_t *temp_group = lv_obj_create(temp_humi_cont);
    lv_obj_set_style_bg_opa(temp_group, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(temp_group, LV_OPA_0, LV_STATE_DEFAULT);

    lv_obj_t *temp_icon = lv_label_create(temp_group);
    lv_label_set_text(temp_icon, "TEMP");
    lv_obj_set_style_text_font(temp_icon, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(temp_icon, lv_color_hex(0xF87171), LV_STATE_DEFAULT);
    lv_obj_align(temp_icon, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *temp_label = lv_label_create(temp_group);
    lv_label_set_text(temp_label, "26.5 C");
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_36, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_align_to(temp_label, temp_icon, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // 湿度图标+数值
    lv_obj_t *humi_group = lv_obj_create(temp_humi_cont);
    lv_obj_set_style_bg_opa(humi_group, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(humi_group, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(humi_group, 50, LV_STATE_DEFAULT);

    lv_obj_t *humi_icon = lv_label_create(humi_group);
    lv_label_set_text(humi_icon, "HUMI");
    lv_obj_set_style_text_font(humi_icon, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(humi_icon, lv_color_hex(0x60A5FA), LV_STATE_DEFAULT);
    lv_obj_align(humi_icon, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *humi_label = lv_label_create(humi_group);
    lv_label_set_text(humi_label, "65 %");
    lv_obj_set_style_text_font(humi_label, &lv_font_montserrat_36, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(humi_label, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_align_to(humi_label, humi_icon, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    // -------------------- 2. 状态 + Auto 行容器 --------------------
    lv_obj_t *status_row = lv_obj_create(left_col);
    lv_obj_set_size(status_row, LV_PCT(100), LV_PCT(35));
    lv_obj_set_style_bg_opa(status_row, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(status_row, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(status_row, 5, LV_STATE_DEFAULT);
    lv_obj_clear_flag(status_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(status_row, LV_ALIGN_BOTTOM_MID, 0, 0);

    // 左：Active 卡片（可变色）
    lv_obj_t *active_card = lv_obj_create(status_row);
    lv_obj_set_size(active_card, LV_PCT(50), LV_PCT(100));
    lv_obj_set_style_bg_color(active_card, lv_color_hex(0xEAB308), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(active_card, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(active_card, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(active_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(active_card, LV_ALIGN_LEFT_MID, 0, 0);

    fan_status_cont = active_card;

    fan_status_label = lv_label_create(active_card);
    lv_label_set_text(fan_status_label, "Active");
    lv_obj_set_style_text_font(fan_status_label, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(fan_status_label, lv_color_hex(0x0F172A), LV_STATE_DEFAULT);
    lv_obj_center(fan_status_label);

    // 右：Auto 卡片（不变色）
    lv_obj_t *auto_card = lv_obj_create(status_row);
    lv_obj_set_size(auto_card, LV_PCT(48), LV_PCT(100));
    lv_obj_set_style_bg_color(auto_card, lv_color_hex(0x1E293B), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(auto_card, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(auto_card, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(auto_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(auto_card, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *auto_title = lv_label_create(auto_card);
    lv_label_set_text(auto_title, "Auto");
    lv_obj_set_style_text_font(auto_title, &lv_font_montserrat_22, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(auto_title, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_align(auto_title, LV_ALIGN_TOP_LEFT, 15, 15);

    lv_obj_t *auto_switch = lv_switch_create(auto_card);
    lv_obj_set_size(auto_switch, 60, 30);
    lv_obj_align(auto_switch, LV_ALIGN_CENTER, 30, 5);
    lv_obj_add_event_cb(auto_switch, lv_auto_light_switch_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_move_foreground(auto_switch);

    // ---- 右列容器 ----
    lv_obj_t *right_col = lv_obj_create(content_row);
    lv_obj_set_size(right_col, LV_PCT(50), LV_PCT(100));
    lv_obj_set_style_pad_all(right_col, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(right_col, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(right_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(right_col, LV_ALIGN_TOP_RIGHT, 0, 0);

    // -------------------- 3. 风扇控制卡片 --------------------
    lv_obj_t *fan_card = lv_obj_create(right_col);
    lv_obj_set_size(fan_card, LV_PCT(100), LV_PCT(45));
    lv_obj_set_style_bg_color(fan_card, lv_color_hex(0x1E293B), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(fan_card, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(fan_card, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(fan_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(fan_card, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *fan_title = lv_label_create(fan_card);
    lv_label_set_text(fan_title, "Fan");
    lv_obj_set_style_text_font(fan_title, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(fan_title, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_align(fan_title, LV_ALIGN_TOP_LEFT, 20, 15);

    lv_obj_t *fan_switch = lv_switch_create(fan_card);
    lv_obj_set_size(fan_switch, 80, 40);
    lv_obj_align(fan_switch, LV_ALIGN_TOP_RIGHT, -20, 15);
    lv_obj_add_event_cb(fan_switch, lv_fan_switch_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_move_foreground(fan_switch);

    lv_obj_t *fan_speed_cont = lv_obj_create(fan_card);
    lv_obj_set_size(fan_speed_cont, LV_PCT(90), 90);
    lv_obj_set_flex_flow(fan_speed_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(fan_speed_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(fan_speed_cont, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(fan_speed_cont, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_align(fan_speed_cont, LV_ALIGN_CENTER, 0, 15);
    lv_obj_set_style_pad_column(fan_speed_cont, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_main_place(fan_speed_cont, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);
    lv_obj_set_style_flex_cross_place(fan_speed_cont, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

    fan_low_btn = lv_btn_create(fan_speed_cont);
    lv_obj_set_size(fan_low_btn, 70, 40);
    lv_obj_set_style_bg_color(fan_low_btn, lv_color_hex(0x334155), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(fan_low_btn, 8, LV_STATE_DEFAULT);
    lv_obj_add_flag(fan_low_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(fan_low_btn, lv_fan_speed_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *fan_low_label = lv_label_create(fan_low_btn);
    lv_label_set_text(fan_low_label, "Low");
    lv_obj_set_style_text_font(fan_low_label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(fan_low_label, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_center(fan_low_label);

    fan_mid_btn = lv_btn_create(fan_speed_cont);
    lv_obj_set_size(fan_mid_btn, 70, 40);
    lv_obj_set_style_bg_color(fan_mid_btn, lv_color_hex(0x334155), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(fan_mid_btn, 8, LV_STATE_DEFAULT);
    lv_obj_add_flag(fan_mid_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_state(fan_mid_btn, LV_STATE_CHECKED);
    lv_obj_add_event_cb(fan_mid_btn, lv_fan_speed_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *fan_mid_label = lv_label_create(fan_mid_btn);
    lv_label_set_text(fan_mid_label, "Mid");
    lv_obj_set_style_text_font(fan_mid_label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(fan_mid_label, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_center(fan_mid_label);

    fan_high_btn = lv_btn_create(fan_speed_cont);
    lv_obj_set_size(fan_high_btn, 70, 40);
    lv_obj_set_style_bg_color(fan_high_btn, lv_color_hex(0x334155), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(fan_high_btn, 8, LV_STATE_DEFAULT);
    lv_obj_add_flag(fan_high_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(fan_high_btn, lv_fan_speed_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *fan_high_label = lv_label_create(fan_high_btn);
    lv_label_set_text(fan_high_label, "High");
    lv_obj_set_style_text_font(fan_high_label, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(fan_high_label, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_center(fan_high_label);

    lv_obj_clear_flag(fan_speed_cont, LV_OBJ_FLAG_CLICKABLE);

    // -------------------- 4. 灯光控制卡片 --------------------
    lv_obj_t *light_card = lv_obj_create(right_col);
    lv_obj_set_size(light_card, LV_PCT(100), LV_PCT(45));
    lv_obj_set_style_bg_color(light_card, lv_color_hex(0x1E293B), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(light_card, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(light_card, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(light_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(light_card, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t *light_title = lv_label_create(light_card);
    lv_label_set_text(light_title, "Light");
    lv_obj_set_style_text_font(light_title, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(light_title, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_align(light_title, LV_ALIGN_TOP_LEFT, 20, 15);

    lv_obj_t *light_switch = lv_switch_create(light_card);
    lv_obj_set_size(light_switch, 80, 40);
    lv_obj_align(light_switch, LV_ALIGN_TOP_RIGHT, -20, 15);
    lv_obj_add_event_cb(light_switch, lv_light_switch_event, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_move_foreground(light_switch);

    lv_obj_t *brightness_slider = lv_slider_create(light_card);
    lv_obj_set_size(brightness_slider, LV_PCT(80), 20);
    lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, 20);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_slider_set_value(brightness_slider, 75, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x334155), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(brightness_slider, 10, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_0, LV_PART_KNOB);

    lv_obj_t *brightness_label = lv_label_create(light_card);
    lv_label_set_text(brightness_label, "Brightness: 75%");
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0x94A3B8), LV_STATE_DEFAULT);
    lv_obj_align(brightness_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(brightness_slider, lv_brightness_slider_event, LV_EVENT_VALUE_CHANGED, brightness_label);

    // ==================== 5. CHAT 卡片 ====================
    chat_card = lv_obj_create(page2_container);
    lv_obj_set_size(chat_card, LV_PCT(100), lv_obj_get_height(lv_scr_act()) * 1 / 6);
    lv_obj_set_style_bg_color(chat_card, lv_color_hex(0x1E293B), LV_STATE_DEFAULT);
    lv_obj_set_style_radius(chat_card, 16, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(chat_card, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_clear_flag(chat_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(chat_card, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(chat_card, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_move_foreground(chat_card);

    lv_obj_t *chat_label = lv_label_create(chat_card);
    lv_label_set_text(chat_label, "CHAT");
    lv_obj_set_style_text_font(chat_label, &lv_font_montserrat_28, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(chat_label, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
    lv_obj_center(chat_label);

    /* 初始化语音控制模块并注册回调，默认开启 */
    voice_control_init(NULL);
    voice_control_register_callback(lv_voice_command_callback);
    voice_control_start_listening();
    ESP_LOGI(HANDLE_TAG, "Voice control registered on smart home page");
}

void lv_app_switch_page(uint8_t page)
{
    if (page >= TOTAL_PAGES)
        return;

    if (page == 0)
    {
        // 切换到主界面：隐藏第二页，显示主界面和时钟
        if (page2_container != NULL)
        {
            lv_obj_add_flag(page2_container, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_clear_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lv_app_ui.app_small_ui.small_cont, LV_OBJ_FLAG_HIDDEN);
        // 显示时钟
        if (lv_app_ui.app_main_ui.main_time_label != NULL && lv_obj_is_valid(lv_app_ui.app_main_ui.main_time_label))
        {
            lv_obj_clear_flag(lv_app_ui.app_main_ui.main_time_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else if (page == 1)
    {
        // 切换到第二页：隐藏主界面和时钟，显示第二页
        lv_obj_add_flag(lv_app_ui.main_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lv_app_ui.app_small_ui.small_cont, LV_OBJ_FLAG_HIDDEN);
        // 隐藏时钟
        if (lv_app_ui.app_main_ui.main_time_label != NULL && lv_obj_is_valid(lv_app_ui.app_main_ui.main_time_label))
        {
            lv_obj_add_flag(lv_app_ui.app_main_ui.main_time_label, LV_OBJ_FLAG_HIDDEN);
        }
        if (page2_container != NULL)
        {
            lv_obj_clear_flag(page2_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_update_layout(page2_container);
        }
    }

    current_page = page;
}

/**************************** 板载实现接口 ********************************/
esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100)
    {
        brightness_percent = 100;
    }
    if (brightness_percent < 0)
    {
        brightness_percent = 0;
    }

    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    return ESP_OK;
}

// ==================== 智能家居控制事件回调函数 ====================

/**
 * @brief 风扇风速档位事件回调（互斥选择）
 */
static void lv_fan_speed_event(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);

    // 取消其他两个按钮的选中状态
    if (btn == fan_low_btn)
    {
        lv_obj_clear_state(fan_mid_btn, LV_STATE_CHECKED);
        lv_obj_clear_state(fan_high_btn, LV_STATE_CHECKED);
        ESP_LOGI(HANDLE_TAG, "风扇风速设为: 低档");
    }
    else if (btn == fan_mid_btn)
    {
        lv_obj_clear_state(fan_low_btn, LV_STATE_CHECKED);
        lv_obj_clear_state(fan_high_btn, LV_STATE_CHECKED);
        ESP_LOGI(HANDLE_TAG, "风扇风速设为: 中档");
    }
    else if (btn == fan_high_btn)
    {
        lv_obj_clear_state(fan_low_btn, LV_STATE_CHECKED);
        lv_obj_clear_state(fan_mid_btn, LV_STATE_CHECKED);
        ESP_LOGI(HANDLE_TAG, "风扇风速设为: 高档");
    }

    // 确保当前按钮处于选中状态
    lv_obj_add_state(btn, LV_STATE_CHECKED);
}

/**
 * @brief 风扇开关事件回调
 */
static void lv_fan_switch_event(lv_event_t *e)
{
    lv_obj_t *switch_obj = lv_event_get_target(e);
    bool state = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
    if (state)
    {
        ESP_LOGI(HANDLE_TAG, "风扇已开启");
        // 这里可以添加实际的风扇控制逻辑
    }
    else
    {
        ESP_LOGI(HANDLE_TAG, "风扇已关闭");
        // 这里可以添加实际的风扇控制逻辑
    }
}

/**
 * @brief 灯光开关事件回调
 */
static void lv_light_switch_event(lv_event_t *e)
{
    lv_obj_t *switch_obj = lv_event_get_target(e);
    bool state = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
    if (state)
    {
        ESP_LOGI(HANDLE_TAG, "灯光已开启");
        // 这里可以添加实际的灯光控制逻辑
    }
    else
    {
        ESP_LOGI(HANDLE_TAG, "灯光已关闭");
        // 这里可以添加实际的灯光控制逻辑
    }
}

/**
 * @brief 亮度滑条事件回调
 */
static void lv_brightness_slider_event(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    lv_obj_t *label = lv_event_get_user_data(e);
    if (label)
    {
        char buf[32];
        lv_snprintf(buf, sizeof(buf), "Brightness: %d%%", (int)val);
        lv_label_set_text(label, buf);
    }
}

/**
 * @brief 自动灯光开关事件回调
 */
static void lv_auto_light_switch_event(lv_event_t *e)
{
    lv_obj_t *switch_obj = lv_event_get_target(e);
    bool state = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
    if (state)
    {
        ESP_LOGI(HANDLE_TAG, "自动灯光控制已启用");
    }
    else
    {
        ESP_LOGI(HANDLE_TAG, "自动灯光控制已禁用");
    }
}

/**
 * @brief 语音指令回调 - 处理语音识别结果, 更新智能家居UI状态
 * @param cmd   : 语音指令类型
 * @param param : 附加参数(0为默认值)
 * @retval 无
 * @note   在LVGL上下文中调用(已持有lvgl_port_lock)
 */
static void lv_voice_command_callback(voice_cmd_t cmd, int param)
{
    (void)param;

    switch (cmd)
    {
    case VOICE_CMD_FAN_ON:
        ESP_LOGI(HANDLE_TAG, "Voice: 风扇开");
        if (fan_low_btn)
            lv_obj_add_state(fan_low_btn, LV_STATE_CHECKED);
        break;

    case VOICE_CMD_FAN_OFF:
        ESP_LOGI(HANDLE_TAG, "Voice: 风扇关");
        if (fan_low_btn)
            lv_obj_clear_state(fan_low_btn, LV_STATE_CHECKED);
        if (fan_mid_btn)
            lv_obj_clear_state(fan_mid_btn, LV_STATE_CHECKED);
        if (fan_high_btn)
            lv_obj_clear_state(fan_high_btn, LV_STATE_CHECKED);
        break;

    case VOICE_CMD_FAN_LOW:
        ESP_LOGI(HANDLE_TAG, "Voice: 风扇低档");
        if (fan_low_btn)
            lv_obj_add_state(fan_low_btn, LV_STATE_CHECKED);
        if (fan_mid_btn)
            lv_obj_clear_state(fan_mid_btn, LV_STATE_CHECKED);
        if (fan_high_btn)
            lv_obj_clear_state(fan_high_btn, LV_STATE_CHECKED);
        break;

    case VOICE_CMD_FAN_MID:
        ESP_LOGI(HANDLE_TAG, "Voice: 风扇中档");
        if (fan_low_btn)
            lv_obj_clear_state(fan_low_btn, LV_STATE_CHECKED);
        if (fan_mid_btn)
            lv_obj_add_state(fan_mid_btn, LV_STATE_CHECKED);
        if (fan_high_btn)
            lv_obj_clear_state(fan_high_btn, LV_STATE_CHECKED);
        break;

    case VOICE_CMD_FAN_HIGH:
        ESP_LOGI(HANDLE_TAG, "Voice: 风扇高档");
        if (fan_low_btn)
            lv_obj_clear_state(fan_low_btn, LV_STATE_CHECKED);
        if (fan_mid_btn)
            lv_obj_clear_state(fan_mid_btn, LV_STATE_CHECKED);
        if (fan_high_btn)
            lv_obj_add_state(fan_high_btn, LV_STATE_CHECKED);
        break;

    case VOICE_CMD_LIGHT_ON:
        ESP_LOGI(HANDLE_TAG, "Voice: 灯光开");
        lv_update_presence_status(true);
        break;

    case VOICE_CMD_LIGHT_OFF:
        ESP_LOGI(HANDLE_TAG, "Voice: 灯光关");
        lv_update_presence_status(false);
        break;

    case VOICE_CMD_BRIGHTNESS_UP:
        ESP_LOGI(HANDLE_TAG, "Voice: 亮度+");
        break;

    case VOICE_CMD_BRIGHTNESS_DOWN:
        ESP_LOGI(HANDLE_TAG, "Voice: 亮度-");
        break;

    case VOICE_CMD_AUTO_ON:
        ESP_LOGI(HANDLE_TAG, "Voice: 自动模式开");
        break;

    case VOICE_CMD_AUTO_OFF:
        ESP_LOGI(HANDLE_TAG, "Voice: 自动模式关");
        break;

    case VOICE_CMD_CHAT:
        ESP_LOGI(HANDLE_TAG, "Voice: CHAT触发");
        if (chat_card && lv_obj_is_valid(chat_card))
        {
            lv_obj_set_style_bg_color(chat_card, lv_color_hex(0xEF4444), LV_STATE_DEFAULT);
        }
        break;

    case VOICE_CMD_WAKEUP_START:
        ESP_LOGI(HANDLE_TAG, "Voice: 唤醒词检测到，chat卡片变蓝");
        if (chat_card && lv_obj_is_valid(chat_card))
        {
            lv_obj_set_style_bg_opa(chat_card, LV_OPA_COVER, LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(chat_card, lv_color_hex(0x3B82F6), LV_STATE_DEFAULT);
        }
        break;

    case VOICE_CMD_WAKEUP_END:
        ESP_LOGI(HANDLE_TAG, "Voice: 唤醒结束，恢复默认");
        if (chat_card && lv_obj_is_valid(chat_card))
        {
            lv_obj_set_style_bg_color(chat_card, lv_color_hex(0x1E293B), LV_STATE_DEFAULT);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief 更新人员状态指示颜色
 * @param has_person: true表示有人（黄色），false表示无人（白色）
 */
void lv_update_presence_status(bool has_person)
{
    if (fan_status_cont != NULL && lv_obj_is_valid(fan_status_cont) &&
        fan_status_label != NULL && lv_obj_is_valid(fan_status_label))
    {
        if (has_person)
        {
            lv_obj_set_style_bg_color(fan_status_cont, lv_color_hex(0xEAB308), LV_STATE_DEFAULT);
            lv_label_set_text(fan_status_label, "Active");
            lv_obj_set_style_text_color(fan_status_label, lv_color_hex(0x0F172A), LV_STATE_DEFAULT);
            ESP_LOGI(HANDLE_TAG, "人员状态: 有人");
        }
        else
        {
            lv_obj_set_style_bg_color(fan_status_cont, lv_color_hex(0x64748B), LV_STATE_DEFAULT);
            lv_label_set_text(fan_status_label, "Inactive");
            lv_obj_set_style_text_color(fan_status_label, lv_color_hex(0xF8FAFC), LV_STATE_DEFAULT);
            ESP_LOGI(HANDLE_TAG, "人员状态: 无人");
        }
    }
}

/**************************** 后台数据处理定时器 ********************************/

/* 定义字符数组用于显示星期 */
char *weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

/**
 * @brief  后台数据处理定时器
 * @param  None
 * @retval None
 */
void lv_background_data_processing_timer(lv_timer_t *timer)
{
    uint8_t key;

    voice_control_process_queue();

    rtc_get_time(); // 获取当前时间

    // 更新时间显示（只显示小时和分钟）
    if (lv_app_ui.app_main_ui.main_time_label && lv_obj_is_valid(lv_app_ui.app_main_ui.main_time_label))
    {
        lv_label_set_text_fmt(lv_app_ui.app_main_ui.main_time_label, "%02d:%02d", calendar.hour, calendar.min);
    }

    /* 更新时间 */
    // lv_label_set_text_fmt(lv_app_ui.app_small_ui.small_cont_timer,"%02d:%02d:%02d",calendar.hour, calendar.min, calendar.sec);
    // if (lv_app_ui.lock_screen.lock_screen_cont != NULL)
    // {
    //     /* 防止锁屏界面被删除 */
    //     lv_obj_update_layout(lv_app_ui.main_cont);
    //     /* 再一次判断 */
    //     if (lv_app_ui.lock_screen.lock_screen_cont != NULL)
    //     {
    //         lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_date,"%02d:%02d:%02d", calendar.hour, calendar.min, calendar.sec);
    //         lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_time,"%04d-%02d-%02d",calendar.year, calendar.month, calendar.date);
    //         lv_label_set_text_fmt(lv_app_ui.lock_screen.lock_screen_week,"%s",weekdays[calendar.week]);
    //     }

    // }

    key = key_scan(0);

    if (key == BOOT_PRES)
    {
        LED1_TOGGLE();
    }
    else if (key == KEY0_PRES)
    {
        LED0_TOGGLE();
    }

    if (lv_general.usb_jtag_check_en == 1)
    {
        if (lv_app_ui.app_small_ui.small_cont_usb != NULL)
        {
            lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_usb, lv_color_make(0, 255, 0), LV_STATE_DEFAULT);
        }
    }
    else if (lv_general.usb_jtag_check_en == 0)
    {
        if (lv_app_ui.app_small_ui.small_cont_usb != NULL)
        {
            lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_usb, lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
        }
    }

    /* USB存储状态跟踪变量（静态，保持上次状态） */
    static bool usb_was_connected = false;

    if (lv_usb_otg_ui.usb_connected == 1)
    {
        if (lv_app_ui.app_small_ui.small_cont_sd != NULL)
        {
            lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_sd, lv_color_make(0, 255, 0), LV_STATE_DEFAULT);
        }

        if (!usb_was_connected)
        {
            ESP_LOGI("USB", "USB drive connected");
            usb_was_connected = true;

            if (lv_general.del_parent != NULL && lv_obj_is_valid(lv_general.del_parent))
            {
                vTaskDelay(pdMS_TO_TICKS(300));
                lv_msgbox_close(lv_general.del_parent);
                lv_general.del_parent = NULL;
            }
        }
    }
    else
    {
        if (usb_was_connected)
        {
            ESP_LOGI("USB", "USB drive disconnected");
            usb_was_connected = false;
        }

        if (lv_app_ui.app_small_ui.small_cont_sd != NULL)
        {
            lv_obj_set_style_text_color(lv_app_ui.app_small_ui.small_cont_sd, lv_color_make(255, 255, 255), LV_STATE_DEFAULT);
        }
    }
}
