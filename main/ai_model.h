// ai_model.h
#ifndef AI_MODEL_H
#define AI_MODEL_H

#ifdef __cplusplus
extern "C"
{
#endif

    // 初始化 AI 模型
    void ai_model_init(void);

    // 传入传感器数据，返回决策：1代表开灯，0代表关灯
    int ai_predict_light(float is_someone_here, float current_lux);

#ifdef __cplusplus
}
#endif

#endif