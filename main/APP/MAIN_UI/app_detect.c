#include "app_detect.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <math.h>

static const char *CSI_TAG = "CSI_HUMAN_DETECT";

/* 全局变量 */
static bool csi_initialized = false;
static bool csi_running = false;
static TaskHandle_t csi_task_handle = NULL;
static SemaphoreHandle_t csi_mutex = NULL;

static csi_config_t csi_config = {
    .sample_count = CSI_DEFAULT_SAMPLE_COUNT,
    .detection_interval_ms = CSI_DEFAULT_DETECTION_INTERVAL,
    .threshold = 0.05};

/* RSSI 历史数据 */
static int8_t rssi_history[CSI_DEFAULT_SAMPLE_COUNT];
static int rssi_index = 0;
static int8_t last_rssi = 0;
static bool rssi_valid = false;

/* 获取当前 WiFi RSSI */
static esp_err_t get_current_rssi(int8_t *rssi)
{
    if (rssi == NULL)
        return ESP_ERR_INVALID_ARG;

    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK)
    {
        *rssi = ap_info.rssi;
        ESP_LOGD(CSI_TAG, "Current RSSI: %d dBm", *rssi);
        return ESP_OK;
    }

    ESP_LOGW(CSI_TAG, "Failed to get RSSI: %s", esp_err_to_name(ret));
    return ret;
}

/* 计算 RSSI 变化率（标准差） */
static float calculate_rssi_change_rate(void)
{
    if (!rssi_valid || rssi_index < 5)
    {
        return 0.0f;
    }

    int count = (rssi_index < csi_config.sample_count) ? rssi_index : csi_config.sample_count;
    float sum = 0.0f;

    /* 计算相邻采样之间的差值 */
    for (int i = 1; i < count; i++)
    {
        int idx1 = (rssi_index - i + csi_config.sample_count) % csi_config.sample_count;
        int idx2 = (rssi_index - i + 1 + csi_config.sample_count) % csi_config.sample_count;
        sum += fabs((float)(rssi_history[idx1] - rssi_history[idx2]));
    }

    /* 计算均值 */
    float mean = sum / (count - 1);

    /* 计算方差 */
    float variance = 0.0f;
    for (int i = 1; i < count; i++)
    {
        int idx1 = (rssi_index - i + csi_config.sample_count) % csi_config.sample_count;
        int idx2 = (rssi_index - i + 1 + csi_config.sample_count) % csi_config.sample_count;
        float diff = fabs((float)(rssi_history[idx1] - rssi_history[idx2])) - mean;
        variance += diff * diff;
    }

    /* 返回标准差 */
    return (float)sqrt(variance / (count - 1));
}

/* 人体检测核心算法（基于 RSSI 变化） */
static csi_detection_result_t csi_algorithm(void)
{
    if (!csi_initialized)
    {
        ESP_LOGE(CSI_TAG, "Detection error: CSI not initialized");
        return CSI_DETECTION_ERROR;
    }

    if (xSemaphoreTake(csi_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(CSI_TAG, "Detection error: Failed to acquire mutex");
        return CSI_DETECTION_ERROR;
    }

    /* 获取当前 RSSI */
    int8_t current_rssi;
    if (get_current_rssi(&current_rssi) != ESP_OK)
    {
        xSemaphoreGive(csi_mutex);
        ESP_LOGE(CSI_TAG, "Detection error: Cannot get RSSI");
        return CSI_DETECTION_ERROR;
    }

    /* 更新 RSSI 历史 */
    rssi_history[rssi_index++] = current_rssi;
    if (rssi_index >= csi_config.sample_count)
    {
        rssi_index = 0;
    }
    rssi_valid = true;

    /* 计算变化率 */
    float change_rate = calculate_rssi_change_rate();
    xSemaphoreGive(csi_mutex);

    ESP_LOGD(CSI_TAG, "RSSI change rate: %.4f, threshold: %.4f",
             change_rate, csi_config.threshold);

    /* 根据阈值判断是否有人体 */
    if (change_rate > csi_config.threshold)
    {
        ESP_LOGI(CSI_TAG, "Human detected: Yes");
        return CSI_HUMAN_DETECTED;
    }
    else
    {
        ESP_LOGI(CSI_TAG, "Human detected: No");
        return CSI_NO_HUMAN;
    }
}

/* 连续检测任务 */
static void csi_detection_task(void *arg)
{
    ESP_LOGI(CSI_TAG, "CSI detection task started (interval: %dms)",
             csi_config.detection_interval_ms);

    while (csi_running)
    {
        csi_detection_result_t result = csi_algorithm();

        if (result == CSI_DETECTION_ERROR)
        {
            ESP_LOGE(CSI_TAG, "Detection error");
        }

        vTaskDelay(pdMS_TO_TICKS(csi_config.detection_interval_ms));
    }

    ESP_LOGI(CSI_TAG, "CSI detection task stopped");
    vTaskDelete(NULL);
}

/* 初始化 CSI 模块 */
esp_err_t csi_init(const csi_config_t *config)
{
    if (csi_initialized)
    {
        ESP_LOGW(CSI_TAG, "CSI already initialized");
        return ESP_OK;
    }

    /* 使用默认配置或用户配置 */
    if (config != NULL)
    {
        memcpy(&csi_config, config, sizeof(csi_config_t));
    }

    /* 创建互斥锁 */
    csi_mutex = xSemaphoreCreateMutex();
    if (csi_mutex == NULL)
    {
        ESP_LOGE(CSI_TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* 初始化 RSSI 历史 */
    memset(rssi_history, 0, sizeof(rssi_history));
    rssi_index = 0;
    rssi_valid = false;

    csi_initialized = true;
    ESP_LOGI(CSI_TAG, "CSI module initialized successfully (using RSSI-based detection)");

    return ESP_OK;
}

/* 反初始化 CSI 模块 */
esp_err_t csi_deinit(void)
{
    if (!csi_initialized)
    {
        ESP_LOGW(CSI_TAG, "CSI not initialized");
        return ESP_OK;
    }

    /* 停止连续检测 */
    csi_stop_continuous_detection();

    /* 删除互斥锁 */
    if (csi_mutex != NULL)
    {
        vSemaphoreDelete(csi_mutex);
        csi_mutex = NULL;
    }

    csi_initialized = false;
    ESP_LOGI(CSI_TAG, "CSI module deinitialized");

    return ESP_OK;
}

/* 单次人体检测 */
csi_detection_result_t csi_detect_human(void)
{
    return csi_algorithm();
}

/* 启动连续检测 */
void csi_start_continuous_detection(void)
{
    if (!csi_initialized)
    {
        ESP_LOGE(CSI_TAG, "CSI not initialized, call csi_init() first");
        return;
    }

    if (csi_running)
    {
        ESP_LOGW(CSI_TAG, "CSI detection already running");
        return;
    }

    csi_running = true;

    /* 创建检测任务（增加栈大小避免溢出） */
    BaseType_t ret = xTaskCreate(
        csi_detection_task,
        "csi_detection",
        4096,
        NULL,
        2,
        &csi_task_handle);

    if (ret != pdPASS)
    {
        ESP_LOGE(CSI_TAG, "Failed to create CSI detection task");
        csi_running = false;
    }
}

/* 停止连续检测 */
void csi_stop_continuous_detection(void)
{
    if (!csi_running)
    {
        return;
    }

    csi_running = false;

    /* 等待任务结束 */
    if (csi_task_handle != NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        csi_task_handle = NULL;
    }
}