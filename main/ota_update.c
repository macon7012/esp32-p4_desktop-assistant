#include "ota_update.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_task_wdt.h"
#include "lcd.h"
#include "esp_lvgl_port.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define OTA_BUF_SIZE 2048 // 减小缓冲区，降低单次Flash写入时间

static const char *TAG = "OTA";

esp_err_t ota_update_start(const char *url)
{
    ESP_LOGI(TAG, "OTA start, URL: %s", url);
    ESP_LOGI(TAG, "Running partition: %s", esp_ota_get_running_partition()->label);

    esp_err_t err = ESP_OK; // 初始化 err 变量
    TaskHandle_t lvgl_task_handle = NULL;

    esp_task_wdt_deinit();

    // 获取LVGL任务句柄并暂停，避免OTA期间屏幕闪烁
    ESP_LOGI(TAG, "Suspending LVGL task...");
    lvgl_task_handle = xTaskGetHandle("lvgl_task");
    if (lvgl_task_handle != NULL)
    {
        vTaskSuspend(lvgl_task_handle);
        ESP_LOGI(TAG, "LVGL task suspended");
    }

    // 关闭背光
    ESP_LOGI(TAG, "Turning off LCD backlight...");
    LCD_BL(0);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .timeout_ms = 60000,
        .buffer_size = 16384,
        .buffer_size_tx = 4096,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "HTTP client init failed");
        goto restore_lvgl;
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        goto restore_lvgl;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0)
    {
        ESP_LOGE(TAG, "HTTP fetch headers failed, content_length=%d", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        err = ESP_FAIL;
        goto restore_lvgl;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL)
    {
        ESP_LOGE(TAG, "No OTA update partition found");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        err = ESP_FAIL;
        goto restore_lvgl;
    }
    ESP_LOGI(TAG, "Writing to partition: %s", update_partition->label);

    esp_ota_handle_t ota_handle = 0;
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        goto restore_lvgl;
    }

    uint8_t *buf = malloc(OTA_BUF_SIZE);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "OTA buf malloc failed");
        esp_ota_abort(ota_handle);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        err = ESP_ERR_NO_MEM;
        goto restore_lvgl;
    }

    int total_read = 0;
    int last_percent = -1;
    esp_err_t ret = ESP_OK;

    while (total_read < content_length)
    {
        int read_len = esp_http_client_read(client, (char *)buf, OTA_BUF_SIZE);
        if (read_len < 0)
        {
            ESP_LOGE(TAG, "HTTP read error: %d", read_len);
            ret = ESP_FAIL;
            break;
        }

        if (read_len > 0)
        {
            err = esp_ota_write(ota_handle, buf, read_len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "OTA write error: %s", esp_err_to_name(err));
                ret = err;
                break;
            }
            total_read += read_len;

            int percent = (total_read * 100) / content_length;
            if (percent != last_percent && percent % 5 == 0)
            {
                ESP_LOGI(TAG, "Progress: %d%% (%d/%d bytes)", percent, total_read, content_length);
                last_percent = percent;
            }

            // 关键修复：Flash写入期间禁用Cache影响SDIO通信
            // 增大延迟让SDIO任务有时间处理积压数据
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    free(buf);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (ret == ESP_OK)
    {
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
            goto restore_lvgl;
        }

        // 设置下次启动分区
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Set boot partition failed: %s", esp_err_to_name(err));
            goto restore_lvgl;
        }
        ESP_LOGI(TAG, "Boot partition set to: %s", update_partition->label);

        ESP_LOGI(TAG, "OTA OK! Restarting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart(); // 重启后LVGL和屏幕自动初始化
    }
    else
    {
        esp_ota_abort(ota_handle);
        ESP_LOGE(TAG, "OTA failed");
        err = ret; // 设置错误码
    }

restore_lvgl:
    // 恢复LVGL任务
    ESP_LOGI(TAG, "Resuming LVGL task...");
    if (lvgl_task_handle != NULL)
    {
        vTaskResume(lvgl_task_handle);
        ESP_LOGI(TAG, "LVGL task resumed");
    }

    // 恢复屏幕背光
    ESP_LOGI(TAG, "Restoring LCD backlight...");
    LCD_BL(1);

    // 恢复看门狗
    esp_task_wdt_config_t twdt_cfg = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << 0) | (1 << 1),
        .trigger_panic = true,
    };
    esp_task_wdt_init(&twdt_cfg);

    return err;
}

static void ota_task(void *arg)
{
    const char *url = (const char *)arg;
    ota_update_start(url);
    free(arg);
    vTaskDelete(NULL);
}

void ota_trigger(const char *url)
{
    char *url_copy = strdup(url);
    if (url_copy == NULL)
    {
        ESP_LOGE(TAG, "OTA trigger: strdup failed");
        return;
    }
    xTaskCreatePinnedToCore(ota_task, "ota_task", 8192, url_copy, 5, NULL, 1);
}