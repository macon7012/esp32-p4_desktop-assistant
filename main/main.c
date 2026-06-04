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
#include "ota_update.h"
#include "esp_task_wdt.h"
#include "driver/ledc.h"

#define OTA_URL_DEFAULT "http://192.168.0.107:8080/comprehensive_routine_70inch_mipilcd.bin"

static char g_ota_url[256] = "";
static bool g_ota_trigger = false;

static void ota_do_update(const char *url)
{
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

	esp_task_wdt_deinit();

	ota_update_start(url);

	esp_task_wdt_config_t twdt_cfg = {
		.timeout_ms = 5000,
		.idle_core_mask = (1 << 0) | (1 << 1),
		.trigger_panic = true,
	};
	esp_task_wdt_init(&twdt_cfg);
	ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 100);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void ota_check_task(void *arg)
{
	vTaskDelay(pdMS_TO_TICKS(5000));

	uint32_t key0_hold_ms = 0;
	const uint32_t LONG_PRESS_MS = 3000;

	while (1) {
		if (KEY0 == 0) {
			key0_hold_ms += 100;
			if (key0_hold_ms >= LONG_PRESS_MS) {
				key0_hold_ms = 0;
				ota_do_update(g_ota_url[0] ? g_ota_url : OTA_URL_DEFAULT);
			}
		} else {
			key0_hold_ms = 0;
		}

		if (g_ota_trigger) {
			g_ota_trigger = false;
			ota_do_update(g_ota_url[0] ? g_ota_url : OTA_URL_DEFAULT);
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void ota_trigger(const char *url)
{
	if (url && url[0]) {
		snprintf(g_ota_url, sizeof(g_ota_url), "%s", url);
	}
	g_ota_trigger = true;
}

static const char *TAG = "main";

#define SMART_LIGHT_INTERVAL_MS 2000

static void smart_light_ai_task(void *arg)
{
	bool bh1750_ok = false;

	vTaskDelay(pdMS_TO_TICKS(3000));

	for (int retry = 0; retry < 5; retry++) {
		if (bh1750_init() == ESP_OK) {
			bh1750_ok = true;
			break;
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

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

		vTaskDelay(pdMS_TO_TICKS(SMART_LIGHT_INTERVAL_MS));
	}
}

void app_main(void)
{
	ai_model_init();

	uint8_t key = 10;
	uint8_t *tx_buf = malloc(8);
	uint8_t *rx_buf = malloc(8);

	usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
		.rx_buffer_size = 1024,
		.tx_buffer_size = 1024,
	};

	led_init();
	key_init();

	key = key_scan(0);

	if (key != KEY0_PRES && key != BOOT_PRES) {
		ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
		lv_general.usb_jtag_check_en = usb_serial_jtag_is_connected() ? 1 : 0;

		for (uint8_t i = 0; i < 8; i++) {
			tx_buf[i] = i;
		}

		twai_init(TWAI_MODE_NO_ACK);
		ESP_ERROR_CHECK(twai_send_data(MSG_ID, tx_buf, 8));
		vTaskDelay(pdMS_TO_TICKS(20));
		twai_receive_data(MSG_ID, rx_buf);

		if (memcmp(tx_buf, rx_buf, 8) == 0) {
			ESP_LOGI(TAG, "twai ok");
		} else {
			ESP_LOGI(TAG, "twai error");
		}

		free(tx_buf);
		free(rx_buf);

		lcd_init();

		if (myi2s_init() != ESP_OK) {
			lcd_show_string(30, 110, 200, 16, 16, "I2S Error", RED);
			while (1) {
				vTaskDelay(pdMS_TO_TICKS(1000));
			}
		}

		while (myes8311_init()) {
			lcd_show_string(30, 110, 200, 16, 16, "ES8311 Error", RED);
			vTaskDelay(pdMS_TO_TICKS(200));
			lcd_fill(30, 110, 239, 126, WHITE);
			vTaskDelay(pdMS_TO_TICKS(200));
		}

		gpio_set_level(GPIO_NUM_11, 1);

		if (mipidev.id == 0x79007) {
			rtc_set_time(2026, 1, 1, 0, 0, 0);

			ledc_config_t *ledc_config = malloc(sizeof(ledc_config_t));
			ledc_config->clk_cfg = LEDC_USE_PLL_DIV_CLK;
			ledc_config->timer_num = LEDC_TIMER_0;
			ledc_config->freq_hz = 5000;
			ledc_config->duty_resolution = LEDC_TIMER_10_BIT;
			ledc_config->channel = LEDC_CHANNEL_0;
			ledc_config->duty = 100;
			ledc_config->gpio_num = GPIO_NUM_23;
			ledc_init(ledc_config);

			wifi_auto_connect();

			dht11_init(GPIO_NUM_10);
			csi_init(NULL);
			smart_home_ctrl_init();
			xTaskCreatePinnedToCore(smart_light_ai_task, "smart_light", 8192, NULL, 3, NULL, 1);
			xTaskCreatePinnedToCore(ota_check_task, "ota_check", 16384, NULL, 2, NULL, 0);

			lvgl_demo();
		}
	} else if (key == BOOT_PRES) {
		wifi_app_flag = 1;
		lcd_init();
		if (mipidev.id == 0x79007) {
			lvgl_demo();
		}
	}
}
