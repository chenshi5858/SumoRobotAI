#ifndef DIRECT_MODEL_H_
#define DIRECT_MODEL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HELLO_WORLD_X_RANGE (2.0f * 3.14159265359f)

typedef struct {
    float x;
    int8_t x_quantized;
    int8_t layer1[16];
    int8_t layer2[16];
    int8_t y_quantized;
    float y;
} direct_model_result_t;

void direct_model_predict(float x, direct_model_result_t *result);
float direct_model_predict_y(float x);

#ifdef __cplusplus
}
#endif

#endif  // DIRECT_MODEL_H_
