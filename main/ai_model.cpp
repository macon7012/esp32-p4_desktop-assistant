// ai_model.cpp
#include "ai_model.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "smart_light_model.h" 

const int kTensorArenaSize = 4 * 1024;
uint8_t *tensor_arena = nullptr;

// 静态全局变量，供预测函数使用
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input = nullptr;
static TfLiteTensor *output = nullptr;

extern "C" void ai_model_init(void)
{
    tensor_arena = (uint8_t *)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
    if (!tensor_arena) {
        tensor_arena = (uint8_t *)malloc(kTensorArenaSize);
    }
    if (!tensor_arena) {
        printf("[AI] 内存分配失败！\n");
        return;
    }

    tflite::InitializeTarget();
    const tflite::Model *model = tflite::GetModel(smart_light_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        printf("[AI] 模型版本不匹配！\n");
        return;
    }

    static tflite::MicroMutableOpResolver<3> resolver;
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddLogistic();

    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    interpreter->AllocateTensors();
    input = interpreter->input(0);
    output = interpreter->output(0);
    printf("[AI] TFLite 模型加载成功，准备就绪！\n");
}

extern "C" int ai_predict_light(float is_someone_here, float current_lux)
{
    if (!interpreter)
        return 0; // 还没初始化则默认关灯

    // 喂入传感器数据
    input->data.f[0] = is_someone_here;
    input->data.f[1] = current_lux;

    // 执行推断
    if (interpreter->Invoke() != kTfLiteOk)
    {
        printf("[AI] 推理失败！\n");
        return 0;
    }

    // 获取概率
    float prob = output->data.f[0];
    printf("[AI] 人员:%.0f, 光照:%.1fLux -> 开灯概率: %.1f%%\n", is_someone_here, current_lux, prob * 100);

    // 如果开灯概率大于 50%，返回 1 (开灯)，否则返回 0 (关灯)
    return prob > 0.5f ? 1 : 0;
}