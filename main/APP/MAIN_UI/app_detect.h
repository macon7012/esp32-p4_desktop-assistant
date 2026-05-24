#ifndef _APP_CSI_H
#define _APP_CSI_H

#include "esp_err.h"

/* 人体检测结果枚举 */
typedef enum {
    CSI_DETECTION_ERROR = -1,  /* 检测错误 */
    CSI_NO_HUMAN = 0,          /* 无人 */
    CSI_HUMAN_DETECTED = 1     /* 检测到人体 */
} csi_detection_result_t;

/* CSI 配置结构体 */
typedef struct {
    int sample_count;           /* 采样数量 */
    int detection_interval_ms;  /* 检测间隔（毫秒）- 降低频率减少资源占用 */
    float threshold;            /* 检测阈值 */
} csi_config_t;

/* 默认配置 - 降低检测频率到2秒一次，减少资源占用 */
#define CSI_DEFAULT_SAMPLE_COUNT        64
#define CSI_DEFAULT_DETECTION_INTERVAL  3000  /* 3秒检测一次 */
#define CSI_DEFAULT_THRESHOLD           0.15

/* 函数声明 */
esp_err_t csi_init(const csi_config_t *config);
esp_err_t csi_deinit(void);
csi_detection_result_t csi_detect_human(void);
void csi_start_continuous_detection(void);
void csi_stop_continuous_detection(void);

#endif /* _APP_CSI_H */
