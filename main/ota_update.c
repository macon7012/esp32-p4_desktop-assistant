#include "ota_update.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OTA";

esp_err_t ota_update_start(const char *url)
{
    ESP_LOGI(TAG, "OTA start, URL: %s", url);
    ESP_LOGI(TAG, "Running partition: %s", esp_ota_get_running_partition()->label);

    esp_http_client_config_t http_config = {
        .url = url,
        .timeout_ms = 30000,
        .buffer_size = 16384,
        .buffer_size_tx = 4096,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        return err;
    }

    int last_reported_percent = -1;

    while (1) {
        err = esp_https_ota_perform(handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(2));

        int total = esp_https_ota_get_image_size(handle);
        int downloaded = esp_https_ota_get_image_len_read(handle);
        if (total > 0) {
            int percent = (downloaded * 100) / total;
            if (percent != last_reported_percent && percent % 10 == 0) {
                ESP_LOGI(TAG, "Progress: %d%% (%d/%d)", percent, downloaded, total);
                last_reported_percent = percent;
            }
        }
    }

    if (err == ESP_OK) {
        err = esp_https_ota_finish(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "OTA OK! Restarting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "OTA finish failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "OTA failed: %s (0x%x)", esp_err_to_name(err), err);
        esp_https_ota_abort(handle);
    }

    return err;
}
