/**
 ******************************************************************************
 * @file        voice_control.c
 * @version     V1.0
 * @brief       语音控制模块实现 - 集成ESP-SR: AFE音频前端 + WakeNet唤醒词 + MultiNet命令词识别
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 * 硬件依赖:     ES8311 编解码器(I2C控制 + I2S数据传输)
 * BSP依赖:      myi2s.c/h (I2S驱动), myes8311.c/h (ES8311编解码器)
 * ESP-SR依赖:   esp_afe_sr (音频前端), esp_wn (唤醒词), esp_mn (命令词)
 * 核心分配:     feed任务 -> Core 0, detect任务 -> Core 1, LVGL UI回调 -> Core 1
 * 架构说明:     采用三任务分离架构
 *               1. sr_feed_task(Core 0): 从I2S RX读取PCM, 计算音频电平, 送入AFE
 *               2. sr_detect_task(Core 1): 从AFE取数据, 唤醒词检测, 命令词识别
 *               3. LVGL定时器(Core 1): 轮询指令队列, 触发回调更新UI
 * 通信机制:     Queue (voice_cmd_queue) 作为detect -> LVGL的指令通道
 ******************************************************************************
 */

#include "voice_control.h"
#include "myi2s.h"
#include "myes8311.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lvgl_port.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_vadn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"
#include "esp_process_sdkconfig.h"

static const char *VOICE_TAG = "voice_ctrl";

static voice_ctrl_cfg_t g_voice_cfg;
static voice_command_callback_t g_cmd_callback = NULL;
static TaskHandle_t g_feed_task_handle = NULL;
static TaskHandle_t g_detect_task_handle = NULL;
static QueueHandle_t g_voice_cmd_queue = NULL;
static volatile bool g_listening = false;
static volatile int g_audio_level = 0;

/* ESP-SR 状态 */
static const esp_afe_sr_iface_t *g_afe_handle = NULL;
static esp_afe_sr_data_t *g_afe_data = NULL;
static srmodel_list_t *g_models = NULL;
static volatile int g_wakeup_flag = 0;

/* 前置声明 */
static esp_err_t voice_control_push_command(voice_cmd_t cmd);

/**
 * @brief       计算PCM音频缓冲区RMS电平
 * @param       buf      : 16位PCM数据缓冲区
 * @param       samples  : 样本数(16位样本个数)
 * @retval      RMS电平值 (0~32767)
 */
static int16_t pcm_rms_level(const int16_t *buf, uint32_t samples)
{
    if (buf == NULL || samples == 0)
    {
        return 0;
    }

    int64_t sum = 0;
    uint32_t count = 0;

    for (uint32_t i = 0; i < samples; i++)
    {
        int32_t sample = buf[i];
        sum += (int64_t)sample * sample;
        count++;
    }

    uint32_t rms = (uint32_t)(sum / count);
    /* 牛顿迭代求平方根近似 */
    uint32_t root = rms;
    if (root > 0)
    {
        uint32_t next = root;
        do
        {
            root = next;
            next = (root + rms / root) / 2;
        } while (next < root);
    }

    return (int16_t)(root > 32767 ? 32767 : root);
}

/**
 * @brief       AFE Feed 任务 - 运行在 Core 0
 *              从I2S RX循环读取PCM, 计算电平, 送入AFE音频前端
 * @param       pvParameters: AFE data指针
 * @retval      无
 */
static void sr_feed_task(void *pvParameters)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)pvParameters;
    int audio_chunksize = g_afe_handle->get_feed_chunksize(afe_data);
    int afe_ch = g_afe_handle->get_feed_channel_num(afe_data);
    int i2s_ch = 2; /* I2S 始终立体声 */

    int16_t *pcm_buf = malloc(audio_chunksize * i2s_ch * sizeof(int16_t));
    if (pcm_buf == NULL)
    {
        ESP_LOGE(VOICE_TAG, "Feed task: failed to allocate PCM buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(VOICE_TAG, "Feed task started on core %d, chunksize=%d, afe_ch=%d, i2s_ch=%d",
             xPortGetCoreID(), audio_chunksize, afe_ch, i2s_ch);

    while (g_listening)
    {
        size_t bytes_read = 0;
        int read_size = audio_chunksize * i2s_ch * sizeof(int16_t);
        int ret = esp_codec_dev_read(codec_handle, pcm_buf, read_size);

        if (ret == ESP_CODEC_DEV_OK)
        {
            uint32_t samples = read_size / sizeof(int16_t);
            g_audio_level = pcm_rms_level(pcm_buf, samples);

            if (afe_ch == 1)
            {
                int half = samples / i2s_ch;
                for (int i = 0; i < half; i++)
                {
                    pcm_buf[i] = pcm_buf[i * i2s_ch];
                }
            }
            g_afe_handle->feed(afe_data, pcm_buf);
        }
        else if (ret != ESP_CODEC_DEV_OK)
        {
            ESP_LOGW(VOICE_TAG, "Codec read error: %d", ret);
            memset(pcm_buf, 0, read_size);
            g_afe_handle->feed(afe_data, pcm_buf);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    free(pcm_buf);
    ESP_LOGI(VOICE_TAG, "Feed task ended");
    vTaskDelete(NULL);
}

/**
 * @brief       AFE Detect 任务 - 运行在 Core 1
 *              从AFE取音频特征, 进行唤醒词检测和命令词识别
 * @param       pvParameters: AFE data指针
 * @retval      无
 */
static void sr_detect_task(void *pvParameters)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)pvParameters;
    int afe_chunksize = g_afe_handle->get_fetch_chunksize(afe_data);

    char *mn_name = esp_srmodel_filter(g_models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    char *wn_name = esp_srmodel_filter(g_models, ESP_WN_PREFIX, NULL);
    ESP_LOGI(VOICE_TAG, "WakeNet model: %s | MultiNet: %s",
             wn_name ? wn_name : "N/A", mn_name ? mn_name : "N/A");

    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, g_voice_cfg.mn_timeout_ms);
    esp_mn_commands_alloc(multinet, model_data);
    esp_mn_commands_clear();
    esp_mn_commands_add(VOICE_CMD_LIGHT_ON, "da kai dian deng");
    esp_mn_commands_add(VOICE_CMD_LIGHT_OFF, "guan bi dian deng");
    esp_mn_commands_add(VOICE_CMD_FAN_ON, "da kai feng shan");
    esp_mn_commands_add(VOICE_CMD_FAN_OFF, "guan bi feng shan");
    esp_mn_commands_add(VOICE_CMD_BRIGHTNESS_UP, "tiao liang yi dian");
    esp_mn_commands_add(VOICE_CMD_BRIGHTNESS_DOWN, "tiao an yi dian");
    esp_mn_commands_add(VOICE_CMD_FAN_DOWN, "feng xiao yi dian");
    esp_mn_commands_add(VOICE_CMD_FAN_UP, "feng da yi dian");
    esp_mn_error_t *err = esp_mn_commands_update();
    if (err && err->num > 0)
    {
        ESP_LOGW(VOICE_TAG, "esp_mn_commands_update: %d phrases rejected", err->num);
        for (int i = 0; i < err->num; i++)
        {
            ESP_LOGW(VOICE_TAG, "  rejected phrase: %s", err->phrases[i]->string ? err->phrases[i]->string : "(null)");
        }
    }
    else
    {
        ESP_LOGI(VOICE_TAG, "esp_mn_commands_update: all phrases accepted");
    }

    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    assert(mu_chunksize == afe_chunksize);

    multinet->print_active_speech_commands(model_data);
    ESP_LOGI(VOICE_TAG, "Detect task started on core %d, afe_chunksize=%d, awaiting wake word...",
             xPortGetCoreID(), afe_chunksize);

    while (g_listening)
    {
        afe_fetch_result_t *res = g_afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            ESP_LOGE(VOICE_TAG, "AFE fetch error!");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED)
        {
            ESP_LOGI(VOICE_TAG, "=== WAKE WORD DETECTED ===");
            multinet->clean(model_data);
        }

        if (res->raw_data_channels == 1 && res->wakeup_state == WAKENET_DETECTED)
        {
            g_wakeup_flag = 1;
            voice_control_push_command(VOICE_CMD_WAKEUP_START);
            ESP_LOGI(VOICE_TAG, ">>> Wakeup confirmed! Start listening for command...");
        }
        else if (res->raw_data_channels > 1 && res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            g_wakeup_flag = 1;
            voice_control_push_command(VOICE_CMD_WAKEUP_START);
            ESP_LOGI(VOICE_TAG, "Channel verified, ch=%d", res->trigger_channel_id);
            ESP_LOGI(VOICE_TAG, ">>> Wakeup confirmed! Start listening for command...");
        }

        if (g_wakeup_flag == 1)
        {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING)
            {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++)
                {
                    ESP_LOGI(VOICE_TAG, "  [VoiceCMD] TOP%d: cmd_id=%d phrase=%d text=\"%s\" prob=%.3f",
                             i + 1, mn_result->command_id[i], mn_result->phrase_id[i],
                             mn_result->string, mn_result->prob[i]);
                }
                int cmd_id = mn_result->command_id[0];
                voice_control_push_command((voice_cmd_t)cmd_id);
                ESP_LOGI(VOICE_TAG, "-----------listening-----------");
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                ESP_LOGI(VOICE_TAG, "Timeout, partial: %s", mn_result->string);
                g_afe_handle->enable_wakenet(afe_data);
                g_wakeup_flag = 0;
                voice_control_push_command(VOICE_CMD_WAKEUP_END);
                ESP_LOGI(VOICE_TAG, "-----------awaits wake word-----------");
                continue;
            }
        }
    }

    if (model_data)
    {
        multinet->destroy(model_data);
        model_data = NULL;
    }
    ESP_LOGI(VOICE_TAG, "Detect task ended");
    vTaskDelete(NULL);
}

/**
 * @brief       推送语音指令到队列 (由语音识别结果触发)
 * @param       cmd : 语音指令
 * @retval      ESP_OK:成功; 其他:失败
 */
esp_err_t voice_control_push_command(voice_cmd_t cmd)
{
    if (g_voice_cmd_queue == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(g_voice_cmd_queue, &cmd, pdMS_TO_TICKS(10)) != pdPASS)
    {
        ESP_LOGW(VOICE_TAG, "Command queue full, dropping cmd=%d", cmd);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief       初始化语音控制模块 (在app_main中调用, 在myes8311_init之后)
 *              加载ESP-SR模型, 创建AFE音频前端
 * @param       cfg: 语音控制配置参数(可为NULL使用默认值)
 * @retval      ESP_OK:初始化成功; 其他:错误代码
 */
esp_err_t voice_control_init(voice_ctrl_cfg_t *cfg)
{
    if (g_voice_cmd_queue != NULL)
    {
        ESP_LOGW(VOICE_TAG, "Voice control already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* 加载配置 */
    if (cfg != NULL)
    {
        memcpy(&g_voice_cfg, cfg, sizeof(voice_ctrl_cfg_t));
    }
    else
    {
        g_voice_cfg = (voice_ctrl_cfg_t)VOICE_CTRL_INIT_CONFIG();
    }

    /* 检查BSP是否已初始化 */
    if (rx_handle == NULL)
    {
        ESP_LOGE(VOICE_TAG, "I2S RX handle is NULL, please call myi2s_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (codec_handle == NULL)
    {
        ESP_LOGE(VOICE_TAG, "Codec handle is NULL, please call myes8311_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    /* 加载 ESP-SR 模型 */
    g_models = esp_srmodel_init(g_voice_cfg.model_path);
    if (g_models == NULL)
    {
        ESP_LOGE(VOICE_TAG, "Failed to load SR models from %s", g_voice_cfg.model_path);
        return ESP_FAIL;
    }
    ESP_LOGI(VOICE_TAG, "SR models loaded from %s", g_voice_cfg.model_path);

    /* 创建 AFE 配置 - "MR" = 1 Mic + 1 Reference(AEC), 与 esp32p4-function-ev 板一致 */
    afe_config_t *afe_cfg = afe_config_init("M", g_models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_cfg->afe_ringbuf_size = 48;
    afe_cfg->vad_min_noise_ms = g_voice_cfg.vad_min_noise_ms;

    /* ESP32-P4 上 WebRTC VAD 不兼容, 用 VADNet 神经网络 VAD 替代 */
    char *vad_name = esp_srmodel_filter(g_models, ESP_VADN_PREFIX, NULL);
    if (vad_name)
    {
        afe_cfg->vad_model_name = vad_name;
        ESP_LOGI(VOICE_TAG, "VADNet model selected: %s", vad_name);
    }
    else
    {
        ESP_LOGW(VOICE_TAG, "VADNet model NOT found, falling back to WebRTC VAD");
    }
    /* memory_alloc_mode 使用默认值, 与 COMMAND 参考代码保持一致 */

    /* 创建 AFE 实例 */
    g_afe_handle = esp_afe_handle_from_config(afe_cfg);
    g_afe_data = g_afe_handle->create_from_config(afe_cfg);
    afe_config_free(afe_cfg);

    if (g_afe_data == NULL)
    {
        ESP_LOGE(VOICE_TAG, "Failed to create AFE data");
        return ESP_FAIL;
    }

    /* P4 上 VAD (WebRTC/Net) 均有 BUG 不触发, 禁用 VAD 让音频直通 WakeNet */
    ESP_LOGI(VOICE_TAG, "Disabling built-in VAD on P4...");
    g_afe_handle->disable_vad(g_afe_data);
    ESP_LOGI(VOICE_TAG, "AFE created, ringbuf=48, vad_noise=%dms, VAD=OFF",
             g_voice_cfg.vad_min_noise_ms);

    /* 打印 AFE 关键参数 */
    int feed_chunksize = g_afe_handle->get_feed_chunksize(g_afe_data);
    int feed_ch = g_afe_handle->get_feed_channel_num(g_afe_data);
    int fetch_chunksize = g_afe_handle->get_fetch_chunksize(g_afe_data);
    ESP_LOGI(VOICE_TAG, "AFE config: feed_chunksize=%d, feed_ch=%d, fetch_chunksize=%d",
             feed_chunksize, feed_ch, fetch_chunksize);

    /* 创建指令队列 */
    g_voice_cmd_queue = xQueueCreate(8, sizeof(voice_cmd_t));
    if (g_voice_cmd_queue == NULL)
    {
        ESP_LOGE(VOICE_TAG, "Failed to create command queue");
        return ESP_ERR_NO_MEM;
    }

    /* 设置麦克风初始增益 (30dB作为起始值) */
    int ret = esp_codec_dev_set_in_gain(codec_handle, 42.0f);
    if (ret != ESP_CODEC_DEV_OK)
    {
        ESP_LOGW(VOICE_TAG, "Failed to set mic gain, ret=%d", ret);
    }

    ESP_LOGI(VOICE_TAG, "Voice control initialized, sr=%lu, frame=%ums",
             g_voice_cfg.sample_rate, g_voice_cfg.frame_ms);

    return ESP_OK;
}

/**
 * @brief       反初始化语音控制模块, 释放资源
 * @param       无
 * @retval      ESP_OK:成功; 其他:错误代码
 */
esp_err_t voice_control_deinit(void)
{
    voice_control_stop_listening();

    if (g_voice_cmd_queue != NULL)
    {
        vQueueDelete(g_voice_cmd_queue);
        g_voice_cmd_queue = NULL;
    }

    if (g_afe_data != NULL)
    {
        g_afe_handle->destroy(g_afe_data);
        g_afe_data = NULL;
    }

    if (g_models != NULL)
    {
        esp_srmodel_deinit(g_models);
        g_models = NULL;
    }

    g_cmd_callback = NULL;
    g_audio_level = 0;
    g_wakeup_flag = 0;

    ESP_LOGI(VOICE_TAG, "Voice control deinitialized");
    return ESP_OK;
}

/**
 * @brief       注册语音指令回调函数 (由handle.c调用, 在LVGL上下文中处理UI更新)
 * @param       cb : 回调函数指针
 * @retval      ESP_OK:成功; 其他:错误代码
 */
esp_err_t voice_control_register_callback(voice_command_callback_t cb)
{
    g_cmd_callback = cb;
    ESP_LOGI(VOICE_TAG, "Command callback registered: %p", cb);
    return ESP_OK;
}

/**
 * @brief       开始语音采集与识别 (启用I2S接收, 创建feed+detect双任务)
 * @param       无
 * @retval      ESP_OK:成功; 其他:错误代码
 */
esp_err_t voice_control_start_listening(void)
{
    if (g_listening)
    {
        ESP_LOGW(VOICE_TAG, "Already listening");
        return ESP_ERR_INVALID_STATE;
    }

    if (rx_handle == NULL)
    {
        ESP_LOGE(VOICE_TAG, "I2S RX not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (g_afe_data == NULL)
    {
        ESP_LOGE(VOICE_TAG, "AFE not initialized, call voice_control_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    /* 与 bsp_board 一致: codec device 已管理 I2S RX, 不再手动 disable/enable */
    g_listening = true;
    g_audio_level = 0;
    g_wakeup_flag = 0;

    /* 创建 detect 任务(Core 1, 高优先级) */
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        sr_detect_task,
        "sr_detect",
        24 * 1024,
        (void *)g_afe_data,
        5,
        &g_detect_task_handle,
        1 /* Core 1 */
    );

    if (task_ret != pdPASS)
    {
        ESP_LOGE(VOICE_TAG, "Failed to create detect task");
        g_listening = false;
        i2s_channel_disable(rx_handle);
        return ESP_ERR_NO_MEM;
    }

    /* 创建 feed 任务(Core 1, 与 detect 同核避免 PIE 协处理器跨核冲突) */
    task_ret = xTaskCreatePinnedToCore(
        sr_feed_task,
        "sr_feed",
        10 * 1024,
        (void *)g_afe_data,
        4,
        &g_feed_task_handle,
        1 /* Core 1 -- PIE 协处理器要求同核 */
    );

    if (task_ret != pdPASS)
    {
        ESP_LOGE(VOICE_TAG, "Failed to create feed task");
        g_listening = false;
        i2s_channel_disable(rx_handle);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(VOICE_TAG, "Listening started (feed:Core1, detect:Core1)");
    return ESP_OK;
}

/**
 * @brief       停止语音采集与识别
 * @param       无
 * @retval      ESP_OK:成功; 其他:错误代码
 */
esp_err_t voice_control_stop_listening(void)
{
    g_listening = false;

    /* 等待 feed 任务结束 */
    int timeout = 50;
    while (g_feed_task_handle != NULL && timeout-- > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (g_feed_task_handle != NULL)
    {
        vTaskDelete(g_feed_task_handle);
        g_feed_task_handle = NULL;
    }

    /* 等待 detect 任务结束 */
    timeout = 50;
    while (g_detect_task_handle != NULL && timeout-- > 0)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (g_detect_task_handle != NULL)
    {
        vTaskDelete(g_detect_task_handle);
        g_detect_task_handle = NULL;
    }

    /* 禁用I2S RX通道 */
    if (rx_handle != NULL)
    {
        i2s_channel_disable(rx_handle);
    }

    g_audio_level = 0;
    g_wakeup_flag = 0;

    ESP_LOGI(VOICE_TAG, "Listening stopped");
    return ESP_OK;
}

/**
 * @brief       查询是否正在采集
 * @param       无
 * @retval      true:正在采集; false:已停止
 */
bool voice_control_is_running(void)
{
    return g_listening;
}

/**
 * @brief       获取当前音频电平 (用于UI音量条显示)
 * @param       无
 * @retval      音频RMS电平值 (0~32767)
 */
int voice_control_get_audio_level(void)
{
    return g_audio_level;
}

/**
 * @brief       轮询处理语音指令队列 (在LVGL定时器回调中调用, Core 1)
 *              此函数应在lvgl上下文(已获取lvgl_port_lock)中调用
 * @param       无
 * @retval      无
 */
void voice_control_process_queue(void)
{
    voice_cmd_t cmd = VOICE_CMD_NONE;

    if (g_voice_cmd_queue == NULL)
    {
        return;
    }

    while (xQueueReceive(g_voice_cmd_queue, &cmd, 0) == pdPASS)
    {
        if (cmd == VOICE_CMD_NONE)
        {
            continue;
        }

        ESP_LOGI(VOICE_TAG, "Processing voice command: %d", cmd);

        if (g_cmd_callback != NULL)
        {
            g_cmd_callback(cmd, 0);
        }
    }
}