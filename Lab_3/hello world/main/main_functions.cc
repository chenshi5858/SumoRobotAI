#include "main_functions.h"

#include <math.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "constants.h"
#include "lab3_tfmicro_config.h"
#include "model.h"

namespace {

const char *TAG = "LAB3_TFMICRO";

const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;
int inference_count = 0;
bool initialized = false;

constexpr int kTensorArenaSize = 2000;
uint8_t tensor_arena[kTensorArenaSize];

void ConfigureMeasureGpio()
{
#if MEASURE_GPIO >= 0
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << MEASURE_GPIO;
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(MEASURE_GPIO, 0);
    ESP_LOGI(TAG, "GPIO %d activo para ventana de medicion de potencia", MEASURE_GPIO);
#else
    ESP_LOGI(TAG, "Medicion por GPIO desactivada. Cambiar MEASURE_GPIO en lab3_tfmicro_config.h para habilitarla.");
#endif
}

float BenchmarkInvokeUs(int8_t x_quantized)
{
#if MEASURE_GPIO >= 0
    gpio_set_level(MEASURE_GPIO, 1);
#endif

    const int64_t start_us = esp_timer_get_time();
    for (int i = 0; i < BENCHMARK_REPEATS; ++i) {
        input->data.int8[0] = x_quantized;
        TfLiteStatus invoke_status = interpreter->Invoke();
        if (invoke_status != kTfLiteOk) {
            MicroPrintf("Invoke failed");
            break;
        }
    }
    const int64_t elapsed_us = esp_timer_get_time() - start_us;

#if MEASURE_GPIO >= 0
    gpio_set_level(MEASURE_GPIO, 0);
#endif

    return static_cast<float>(elapsed_us) / static_cast<float>(BENCHMARK_REPEATS);
}

}  // namespace

void setup()
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    ConfigureMeasureGpio();

    ESP_LOGI(TAG, "Modelo hello_world con TensorFlow Lite Micro");
    ESP_LOGI(TAG, "Promedio de tiempo con %d llamadas a interpreter->Invoke() por punto", BENCHMARK_REPEATS);

    model = tflite::GetModel(g_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf("Model schema version %d != supported version %d",
                    model->version(),
                    TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::MicroMutableOpResolver<1> resolver;
    if (resolver.AddFullyConnected() != kTfLiteOk) {
        MicroPrintf("AddFullyConnected() failed");
        return;
    }

    static tflite::MicroInterpreter static_interpreter(
        model,
        resolver,
        tensor_arena,
        kTensorArenaSize
    );
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        MicroPrintf("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
    inference_count = 0;
    initialized = true;

    printf("source,x,y_model,y_sin,avg_inference_us,q_input,q_output\n");
}

void loop()
{
    if (!initialized) {
        ESP_LOGE(TAG, "TF Micro no inicializo correctamente");
        return;
    }

    const float position = static_cast<float>(inference_count) /
                           static_cast<float>(kInferencesPerCycle);
    const float x = position * kXrange;

    const int8_t x_quantized = static_cast<int8_t>(
        (x / input->params.scale) + static_cast<float>(input->params.zero_point)
    );

    const float average_us = BenchmarkInvokeUs(x_quantized);

    const int8_t y_quantized = output->data.int8[0];
    const float y = (static_cast<float>(y_quantized) -
                     static_cast<float>(output->params.zero_point)) *
                    output->params.scale;
    const float y_sin = sinf(x);

    printf(
        "tfmicro,%.6f,%.6f,%.6f,%.3f,%d,%d\n",
        static_cast<double>(x),
        static_cast<double>(y),
        static_cast<double>(y_sin),
        static_cast<double>(average_us),
        static_cast<int>(x_quantized),
        static_cast<int>(y_quantized)
    );

    inference_count += 1;
    if (inference_count >= kInferencesPerCycle) {
        inference_count = 0;
    }
}
