#include <math.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "direct_model.h"
#include "lab3_config.h"

static const char *TAG = "LAB3_1_1";
static volatile float s_benchmark_sink;

static void configure_measure_gpio(void)
{
#if MEASURE_GPIO >= 0
    gpio_config_t config = {
        .pin_bit_mask = 1ULL << MEASURE_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(MEASURE_GPIO, 0);
    ESP_LOGI(TAG, "GPIO %d activo para ventana de medicion de potencia", MEASURE_GPIO);
#else
    ESP_LOGI(TAG, "Medicion por GPIO desactivada. Cambiar MEASURE_GPIO en lab3_config.h para habilitarla.");
#endif
}

static float benchmark_inference_us(float x, direct_model_result_t *last_result)
{
    direct_model_result_t result = {0};

#if MEASURE_GPIO >= 0
    gpio_set_level(MEASURE_GPIO, 1);
#endif

    const int64_t start_us = esp_timer_get_time();
    for (int i = 0; i < BENCHMARK_REPEATS; ++i) {
        direct_model_predict(x, &result);
        s_benchmark_sink += result.y;
    }
    const int64_t elapsed_us = esp_timer_get_time() - start_us;

#if MEASURE_GPIO >= 0
    gpio_set_level(MEASURE_GPIO, 0);
#endif

    if (last_result != NULL) {
        *last_result = result;
    }

    return (float)elapsed_us / (float)BENCHMARK_REPEATS;
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    configure_measure_gpio();

    ESP_LOGI(TAG, "Implementacion directa del modelo hello_world");
    ESP_LOGI(TAG, "Capas: dense 1x16 + ReLU, dense 16x16 + ReLU, dense 16x1");
    ESP_LOGI(TAG, "Promedio de tiempo con %d inferencias por punto", BENCHMARK_REPEATS);

    printf("source,x,y_model,y_sin,avg_inference_us,q_input,q_output\n");

    int inference_count = 0;
    while (1) {
        const float position = (float)inference_count / (float)INFERENCES_PER_CYCLE;
        const float x = position * HELLO_WORLD_X_RANGE;

        direct_model_result_t result = {0};
        const float average_us = benchmark_inference_us(x, &result);
        const float y_sin = sinf(x);

        printf(
            "direct,%.6f,%.6f,%.6f,%.3f,%d,%d\n",
            (double)x,
            (double)result.y,
            (double)y_sin,
            (double)average_us,
            result.x_quantized,
            result.y_quantized
        );

        inference_count += 1;
        if (inference_count >= INFERENCES_PER_CYCLE) {
            inference_count = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(OUTPUT_PERIOD_MS));
    }
}
