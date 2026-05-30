#include "direct_model.h"

#include <math.h>
#include <stddef.h>

#define INPUT_SCALE 0.02457397617f
#define INPUT_ZERO_POINT (-128)

#define L1_WEIGHT_SCALE 0.004224280361f
#define L1_OUTPUT_SCALE 0.01219841838f
#define L1_OUTPUT_ZERO_POINT (-128)

#define L2_WEIGHT_SCALE 0.01263460517f
#define L2_OUTPUT_SCALE 0.005884238984f
#define L2_OUTPUT_ZERO_POINT (-128)

#define L3_WEIGHT_SCALE 0.008502001874f
#define OUTPUT_SCALE 0.008472034708f
#define OUTPUT_ZERO_POINT 4

static const int8_t kLayer1Weights[16] = {
    117, 28, 17, -31, 12, -127, -91, 66,
    -2, -43, -44, -78, 97, 120, 25, -33
};

static const int32_t kLayer1Bias[16] = {
    4, 2935, -2477, 0, 3191, 0, 0, 1747,
    0, 0, 8562, 0, 1839, -2713, -4044, 0
};

static const int8_t kLayer2Weights[16 * 16] = {
    -18, -4, 0, -20, 5, 23, -17, -20,
    -26, -8, 3, 1, 0, -6, -8, -11,
    -36, -21, 39, 20, -15, -34, -30, -37,
    -16, -34, 49, 6, 2, -26, -18, -7,
    0, 22, 7, -32, -2, -1, -23, 6,
    -25, -17, -127, 27, 24, -22, -55, 1,
    15, 0, -38, -9, 14, -20, 19, 31,
    4, 19, -76, -26, -3, 6, -71, -32,
    13, -20, -16, -34, -21, -9, 5, 38,
    26, -28, 111, 26, -22, 30, 53, -33,
    26, -13, -15, 25, 15, 3, 27, -31,
    -34, 19, -10, 25, -1, -10, 27, 24,
    -16, 28, -38, 27, 27, 32, -27, 26,
    -11, -1, -106, 11, 0, 1, -51, -34,
    13, -10, 22, -29, -19, -4, 14, -23,
    -6, -21, 92, -4, 29, 2, 91, -30,
    -31, -11, 21, -20, -12, 0, 19, 5,
    -20, 12, 29, 20, 14, -25, 11, -12,
    25, 0, -41, 5, 39, 2, 21, -22,
    -22, 2, -101, 0, 12, -6, -24, -22,
    -3, 0, 20, -3, 11, 2, -17, -18,
    6, -18, 1, 13, 6, -26, -9, 17,
    -9, 9, -8, -15, 33, -1, 14, -13,
    -20, 18, 38, 29, -14, -23, 40, 24,
    -32, -5, -13, -12, 5, 29, 29, -5,
    -3, 30, -4, 17, -24, 7, 9, 3,
    18, -14, 54, -5, -36, 28, -7, -17,
    -13, -25, 111, 12, 29, 0, 69, -3,
    14, -16, 11, 25, 26, -6, -32, 25,
    31, 19, 54, 28, 18, -21, 59, 12,
    -76, -53, -26, 19, -6, -21, -15, 6,
    28, -6, 24, -27, -21, -53, 12, -12
};

static const int32_t kLayer2Bias[16] = {
    0, 1205, 2680, 1581, -1935, 0, 2714, -2050,
    1294, 2516, -441, 1206, 0, -2132, -1717, 1354
};

static const int8_t kLayer3Weights[16] = {
    33, -91, -117, -54, 94, 29, -50, 66,
    -99, -50, 31, -80, -33, 84, 47, -127
};

static const int32_t kLayer3Bias[1] = {
    -4212
};

static int8_t clamp_int8(int32_t value)
{
    if (value > 127) {
        return 127;
    }
    if (value < -128) {
        return -128;
    }
    return (int8_t)value;
}

static int8_t quantize_input(float x)
{
    const float q = (x / INPUT_SCALE) + (float)INPUT_ZERO_POINT;
    return clamp_int8((int32_t)q);
}

static int8_t requantize(int32_t accumulator, float effective_scale, int32_t output_zero_point)
{
    const int32_t quantized = (int32_t)lrintf((float)accumulator * effective_scale) + output_zero_point;
    return clamp_int8(quantized);
}

static void fully_connected(
    const int8_t *input,
    int input_size,
    int32_t input_zero_point,
    float input_scale,
    const int8_t *weights,
    float weight_scale,
    const int32_t *bias,
    int output_size,
    float output_scale,
    int32_t output_zero_point,
    int relu,
    int8_t *output)
{
    const float effective_scale = (input_scale * weight_scale) / output_scale;

    for (int out = 0; out < output_size; ++out) {
        int32_t accumulator = bias[out];

        for (int in = 0; in < input_size; ++in) {
            const int32_t centered_input = (int32_t)input[in] - input_zero_point;
            const int32_t weight = (int32_t)weights[(out * input_size) + in];
            accumulator += centered_input * weight;
        }

        int8_t quantized = requantize(accumulator, effective_scale, output_zero_point);
        if (relu && quantized < output_zero_point) {
            quantized = (int8_t)output_zero_point;
        }
        output[out] = quantized;
    }
}

void direct_model_predict(float x, direct_model_result_t *result)
{
    if (result == NULL) {
        return;
    }

    result->x = x;
    result->x_quantized = quantize_input(x);

    fully_connected(
        &result->x_quantized,
        1,
        INPUT_ZERO_POINT,
        INPUT_SCALE,
        kLayer1Weights,
        L1_WEIGHT_SCALE,
        kLayer1Bias,
        16,
        L1_OUTPUT_SCALE,
        L1_OUTPUT_ZERO_POINT,
        1,
        result->layer1);

    fully_connected(
        result->layer1,
        16,
        L1_OUTPUT_ZERO_POINT,
        L1_OUTPUT_SCALE,
        kLayer2Weights,
        L2_WEIGHT_SCALE,
        kLayer2Bias,
        16,
        L2_OUTPUT_SCALE,
        L2_OUTPUT_ZERO_POINT,
        1,
        result->layer2);

    fully_connected(
        result->layer2,
        16,
        L2_OUTPUT_ZERO_POINT,
        L2_OUTPUT_SCALE,
        kLayer3Weights,
        L3_WEIGHT_SCALE,
        kLayer3Bias,
        1,
        OUTPUT_SCALE,
        OUTPUT_ZERO_POINT,
        0,
        &result->y_quantized);

    result->y = ((float)result->y_quantized - (float)OUTPUT_ZERO_POINT) * OUTPUT_SCALE;
}

float direct_model_predict_y(float x)
{
    direct_model_result_t result = {0};
    direct_model_predict(x, &result);
    return result.y;
}
