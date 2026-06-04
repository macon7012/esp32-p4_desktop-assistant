#ifndef AI_MODEL_H
#define AI_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

void ai_model_init(void);
int ai_predict_light(float is_someone_here, float current_lux);

#ifdef __cplusplus
}
#endif

#endif
