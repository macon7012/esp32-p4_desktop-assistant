

#include "led.h"
#include "lcd.h"
#include "myiic.h"
#include "ledc.h"
#include "myes8311.h"
#include "esp_rtc.h"
#include "lvgl_demo.h"
#include <stdio.h>
#include <stdbool.h>
#include "key.h"
#include "ai_model.h"
#include "driver/usb_serial_jtag.h"
#include "app_wifi.h"
#include "bh1750.h"
#include "dht11.h"
#include "app_detect.h"
#include "smart_home_ctrl.h"

const char *main_twai_tag = "main_twai";

#define SMART_LIGHT_INTERVAL_MS  2000

static void smart_light_ai_task(void *arg)
{
    bool bh1750_ok = false;

    vTaskDelay(pdMS_TO_TICKS(3000));

    for (int retry = 0; retry < 5; retry++) {
        if (bh1750_init() == ESP_OK) {
            bh1750_ok = true;
            break;
        }
        ESP_LOGW("SMART_LIGHT", "BH1750 init retry %d...", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI("SMART_LIGHT", "AI smart light running, bh1750:%s", bh1750_ok ? "OK" : "FAIL");

    while (1) {
        float lux = 0;
        dht11_data_t dht_data = {0};

        if (bh1750_ok) {
            bh1750_read(&lux);
        }
        dht11_read(&dht_data);

        csi_detection_result_t human = csi_detect_human();
        bool is_someone = (human == CSI_HUMAN_DETECTED);

        int light_on = ai_predict_light(is_someone ? 1.0f : 0.0f, lux);

        smart_home_ctrl_update_sensor(dht_data.temperature, dht_data.humidity,
                                      is_someone, light_on ? true : false);

        if (smart_home_ctrl_get_auto_mode()) {
            if (light_on) {
                smart_home_ctrl_light_on();
            } else {
                smart_home_ctrl_light_off();
            }
        }

        ESP_LOGI("SMART_LIGHT", "Human:%s Lux:%.1f T:%.1f H:%.1f -> %s auto:%s",
                 is_someone ? "Y" : "N", lux, dht_data.temperature, dht_data.humidity,
                 light_on ? "ON" : "OFF",
                 smart_home_ctrl_get_auto_mode() ? "Y" : "N");

        vTaskDelay(pdMS_TO_TICKS(SMART_LIGHT_INTERVAL_MS));
    }
}

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
	ai_model_init(); // 初始化AI模型

	uint8_t x = 0;
	uint8_t key = 10;
	uint8_t *tx_buf = malloc(8);
	uint8_t *rx_buf = malloc(8);

	usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
		.rx_buffer_size = 1024,
		.tx_buffer_size = 1024,
	};

	led_init(); /* LED初始化 */
	key_init(); /* 按键初始化 */

	key = key_scan(0);

	if (key != KEY0_PRES && key != BOOT_PRES) /* MIPI屏幕*/
	{
		/*********************************** USB JTAG Test ************************************/
		ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));

		if (usb_serial_jtag_is_connected())
		{
			lv_general.usb_jtag_check_en = 1;
		}
		else
		{
			lv_general.usb_jtag_check_en = 0;
		}

		/************************************** Twai Test ***************************************/
		for (uint8_t i = 0; i < 8; i++)
		{
			tx_buf[i] = i;
		}

		twai_init(TWAI_MODE_NO_ACK);						/* TWAI初始化 */
		ESP_ERROR_CHECK(twai_send_data(MSG_ID, tx_buf, 8)); /* ID = 0x12, 发送8个字节 */
		vTaskDelay(pdMS_TO_TICKS(20));
		twai_receive_data(MSG_ID, rx_buf); /* CAN ID = 0x12, 接收数据查询 */

		if (memcmp(tx_buf, rx_buf, 8) == 0)
		{
			for (uint8_t i = 0; i < 3; i++)
			{
				ESP_LOGI(main_twai_tag, "twai ok");
			}
		}
		else
		{
			for (uint8_t i = 0; i < 3; i++)
			{
				ESP_LOGI(main_twai_tag, "twai error");
			}
		}

		free(tx_buf);
		free(rx_buf);

		lcd_init(); /* MIPI LCD初始化 */

		if (myi2s_init() != ESP_OK) /* i2s初始化 */
		{
			lcd_show_string(30, 110, 200, 16, 16, "I2S Error", RED);
			while (1)
			{
				vTaskDelay(pdMS_TO_TICKS(1000));
			}
		}

		while (myes8311_init()) /* ES8311初始化 */
		{
			lcd_show_string(30, 110, 200, 16, 16, "ES8311 Error", RED);
			vTaskDelay(pdMS_TO_TICKS(200));
			lcd_fill(30, 110, 239, 126, WHITE);
			vTaskDelay(pdMS_TO_TICKS(200));
		}

		gpio_set_level(GPIO_NUM_11, 1); /* 打开喇叭 */

		if (mipidev.id == 0x79007) /* 7寸 1024*600 MIPI屏幕 */
		{
			rtc_set_time(2026, 1, 1, 0, 0, 0); /* 设置默认RTC时间（联网后自动同步） */
			ledc_config_t *ledc_config = malloc(sizeof(ledc_config_t));
			ledc_config->clk_cfg = LEDC_USE_PLL_DIV_CLK;	  /* 启动定时器时，根据给出的分辨率和占空率参数自动选择ledc源时钟 */
			ledc_config->timer_num = LEDC_TIMER_0;			  /* 选择哪个定时器计数（LEDC_TIMER_0~LEDC_TIMER_3） */
			ledc_config->freq_hz = 5000;					  /* 1KHz（系统自动计算分配系数，并提供freq_hz频率给到定时器） */
			ledc_config->duty_resolution = LEDC_TIMER_10_BIT; /* 设置定时器最大计数值（请看技术手册表32.4.1） */
			ledc_config->channel = LEDC_CHANNEL_0;			  /* 设置输出通道（LEDC_CHANNEL_0 ~ LEDC_CHANNEL_7） */
			ledc_config->duty = 100;						  /* 一个周期内占高电平时间(占空比) */
			ledc_config->gpio_num = GPIO_NUM_23;			  /* PWM信号输出那个管脚 */
			ledc_init(ledc_config);

			wifi_auto_connect(); /* 上电自动连接WiFi */

			dht11_init(GPIO_NUM_10);
			csi_init(NULL);
			smart_home_ctrl_init();
			xTaskCreatePinnedToCore(smart_light_ai_task, "smart_light", 4096, NULL, 3, NULL, 1);

			lvgl_demo();
		}
	}
	else if (key == BOOT_PRES) /*wifi APP*/
	{
		wifi_app_flag = 1;

		lcd_init(); /* MIPI LCD初始化 */

		if (mipidev.id == 0x79007) 
		{
			lvgl_demo();
		}
	}
}