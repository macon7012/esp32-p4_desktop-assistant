/**
 ******************************************************************************
 * @file        jpeg.c
 * @version     V1.0
 * @brief       图片解码-jpeg解码 代码
 ******************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:     慧勤智远 ESP32-P4 开发板
 ******************************************************************************
 */

#include "jpeg.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"

static const char *TAG = "jpeg";

uint8_t *rx_buf = NULL;
uint8_t *tx_buf = NULL;
static uint8_t *display_buf[2] = {NULL, NULL};
static size_t display_buf_size = 0;
static int display_buf_idx = 0;
/* Configuration parameters for the JPEG decoder engine */
jpeg_decode_engine_cfg_t decode_eng_cfg = {
    .intr_priority = 0,
    .timeout_ms = 40,
};

/* Configuration parameters for a JPEG decoder image process */
jpeg_decode_cfg_t decode_cfg_rgb = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565, /* RGB565 */
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
};

/* JPEG decoder memory allocation config */
jpeg_decode_memory_alloc_cfg_t rx_mem_cfg = {
    .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER, /* Alloc the picture output buffer, (decompressed format in decoder) */
};
/* JPEG decoder memory allocation config */
jpeg_decode_memory_alloc_cfg_t tx_mem_cfg = {
    .buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER, /* Alloc the picture input buffer, (compressed format in decoder) */
};

TickType_t jpeg_decode(const char *filename, int width, int height, lcd_write_cb lcd_cb)
{
    jpeg_decoder_handle_t jpgd_handle;
    TickType_t startTick, endTick, diffTick;

    startTick = xTaskGetTickCount();

    /* Acquire a JPEG decode engine with the specified configuration */
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &jpgd_handle));

    FIL *fp = (FIL *)malloc(sizeof(FIL));

    if (fp == NULL)
    {
        ESP_LOGE(__FUNCTION__, "Failed to allocate memory for file handle");
        return 0;
    }
    /* Open File */
    esp_err_t ret = f_open(fp, (const TCHAR *)filename, FA_READ);

    if (ret != FR_OK)
    {
        ESP_LOGW(__FUNCTION__, "Failed to open file [%s]", filename);
        free(fp);
        return 0;
    }

    UINT bytes_read;

    /* Get size */
    FILINFO fno;
    ret = f_stat(filename, &fno);

    if (ret != FR_OK)
    {
        ESP_LOGE(TAG, "f_stat error: %d", ret);
        f_close(fp);
        return 0;
    }
    /* File size */
    // size_t jpeg_size = fno.fsize;
    size_t jpeg_size = fno.fsize;

    /* allocate memory space for JPEG decoder */
    size_t tx_buffer_size = 0;
    tx_buf = (uint8_t *)jpeg_alloc_decoder_mem(jpeg_size, &tx_mem_cfg, &tx_buffer_size);
    if (tx_buf == NULL)
    {
        ESP_LOGE(__FUNCTION__, "alloc tx buffer error");
        f_close(fp);
        return 0;
    }
    f_lseek(fp, SEEK_SET);
    /* Read File */
    ret = f_read(fp, tx_buf, jpeg_size, &bytes_read);

    if (jpeg_size != bytes_read)
    {
        ESP_LOGE(TAG, "f_stat error: %d", ret);
        f_close(fp);
        return 0;
    }

    /* Structure for jpeg decode header */
    jpeg_decode_picture_info_t header_info;
    ESP_ERROR_CHECK(jpeg_decoder_get_info(tx_buf, jpeg_size, &header_info));
    ESP_LOGI(__FUNCTION__, "header parsed, width is %" PRId32 ", height is %" PRId32, header_info.width, header_info.height);

    /* allocate memory space for JPEG decoder */
    size_t rx_buffer_size = 0;
    if (rx_buf != NULL)
    {
        heap_caps_free(rx_buf);
        rx_buf = NULL;
    }
    rx_buf = (uint8_t *)jpeg_alloc_decoder_mem(header_info.width * header_info.height * 2 * 10, &rx_mem_cfg, &rx_buffer_size);

    if (rx_buf == NULL)
    {
        ESP_LOGE(__FUNCTION__, "alloc rx buffer error");
        f_close(fp);
        return 0;
    }

    static uint32_t out_size = 0;
    ESP_ERROR_CHECK(jpeg_decoder_process(jpgd_handle, &decode_cfg_rgb, tx_buf, tx_buffer_size, rx_buf, rx_buffer_size, &out_size));

    size_t display_size = header_info.width * header_info.height * 2;
    if (display_buf[0] == NULL || display_buf_size < display_size)
    {
        if (display_buf[0] != NULL)
        {
            heap_caps_free(display_buf[0]);
            display_buf[0] = NULL;
        }
        if (display_buf[1] != NULL)
        {
            heap_caps_free(display_buf[1]);
            display_buf[1] = NULL;
        }
        display_buf[0] = (uint8_t *)heap_caps_malloc(display_size, MALLOC_CAP_DMA);
        display_buf[1] = (uint8_t *)heap_caps_malloc(display_size, MALLOC_CAP_DMA);
        if (display_buf[0] == NULL || display_buf[1] == NULL)
        {
            ESP_LOGE(__FUNCTION__, "alloc display buffer error");
            if (display_buf[0] != NULL)
                heap_caps_free(display_buf[0]);
            if (display_buf[1] != NULL)
                heap_caps_free(display_buf[1]);
            display_buf[0] = NULL;
            display_buf[1] = NULL;
            f_close(fp);
            heap_caps_free(rx_buf);
            rx_buf = NULL;
            heap_caps_free(tx_buf);
            tx_buf = NULL;
            ESP_ERROR_CHECK(jpeg_del_decoder_engine(jpgd_handle));
            return 0;
        }
        display_buf_size = display_size;
    }

    esp_cache_msync(rx_buf, display_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

    memcpy(display_buf[display_buf_idx], rx_buf, display_size);

    esp_cache_msync(display_buf[display_buf_idx], display_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    heap_caps_free(rx_buf);
    rx_buf = NULL;
    heap_caps_free(tx_buf);
    tx_buf = NULL;

    f_close(fp);
    ESP_ERROR_CHECK(jpeg_del_decoder_engine(jpgd_handle));

    lcd_cb(header_info.width, header_info.height, display_buf[display_buf_idx]);

    display_buf_idx = 1 - display_buf_idx;

    endTick = xTaskGetTickCount();
    diffTick = endTick - startTick;
    ESP_LOGI(__FUNCTION__, "elapsed time[ms]:%" PRIu32, diffTick * portTICK_PERIOD_MS);

    return diffTick;
}

void jpeg_cleanup_buf(void)
{
    if (rx_buf != NULL)
    {
        heap_caps_free(rx_buf);
        rx_buf = NULL;
    }
    if (tx_buf != NULL)
    {
        heap_caps_free(tx_buf);
        tx_buf = NULL;
    }
    if (display_buf[0] != NULL)
    {
        heap_caps_free(display_buf[0]);
        display_buf[0] = NULL;
    }
    if (display_buf[1] != NULL)
    {
        heap_caps_free(display_buf[1]);
        display_buf[1] = NULL;
    }
    display_buf_size = 0;
    display_buf_idx = 0;
}
