#include <stdio.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "esp_adc/adc_continuous.h"

#include "esp_dsp.h"

static const char *TAG = "ROBOT_AUDIO";

/* ================== CONFIG ================== */

// ADC (micrófono)
#define MIC_ADC_CHANNEL ADC_CHANNEL_1   // GPIO2 recomendado
#define SAMPLE_RATE     16000
#define FFT_SIZE        1024

// Motores (ajusta a tu conexión real)
#define MOTOR_A_PIN1 6
#define MOTOR_A_PIN2 7
#define MOTOR_B_PIN1 16
#define MOTOR_B_PIN2 17
#define MOTOR_A_ENA  9 
#define MOTOR_B_ENB  10

// Frecuencias de control
#define FREQ_FORWARD   500.0f
#define FREQ_BACKWARD  1000.0f
#define FREQ_LEFT      1500.0f
#define FREQ_RIGHT     2000.0f
#define FREQ_TOL       100.0f

/* ================== VARIABLES ================== */

static float fft_input[FFT_SIZE * 2];
static float window[FFT_SIZE];

static adc_continuous_handle_t adc_handle = NULL;

/* ================== MOTORES ================== */

void init_motors() {
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << MOTOR_A_PIN1) |
            (1ULL << MOTOR_A_PIN2) |
            (1ULL << MOTOR_B_PIN1) |
            (1ULL << MOTOR_B_PIN2) |
            (1ULL << MOTOR_A_ENA)  |
            (1ULL << MOTOR_B_ENB),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_set_level(MOTOR_A_ENA, 1);
    gpio_set_level(MOTOR_B_ENB, 1);
}

void move_robot(const char* cmd) {
    if (strcmp(cmd, "FORWARD") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
    } else if (strcmp(cmd, "BACKWARD") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 1);
    } else if (strcmp(cmd, "LEFT") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
    } else if (strcmp(cmd, "RIGHT") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 1);
    } else {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 0);
    }
}

/* ================== ADC ================== */

void init_adc() {
    adc_continuous_handle_cfg_t cfg = {
        .max_store_buf_size = FFT_SIZE * sizeof(adc_digi_output_data_t) * 2,
        .conv_frame_size = FFT_SIZE * sizeof(adc_digi_output_data_t),
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&cfg, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = MIC_ADC_CHANNEL,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = &pattern;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

/* ================== MAIN ================== */

void app_main(void) {
    ESP_LOGI(TAG, "Inicio sistema audio + motores");

    init_motors();
    init_adc();

    // FFT init
    dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    dsps_wind_hann_f32(window, FFT_SIZE);

    static uint8_t result[1024 * sizeof(adc_digi_output_data_t)];
    uint32_t out_len = 0;

    while (1) {
        if (adc_continuous_read(adc_handle, result,
            sizeof(result), &out_len, portMAX_DELAY) == ESP_OK) {

            // Preparar datos FFT
            for (int i = 0; i < FFT_SIZE; i++) {
                adc_digi_output_data_t *p =
                    (adc_digi_output_data_t*)&result[i * sizeof(adc_digi_output_data_t)];

                float val = (float)p->type2.data - 2048.0f;

                fft_input[2*i]     = val * window[i];
                fft_input[2*i + 1] = 0;
            }

            // FFT
            dsps_fft2r_fc32(fft_input, FFT_SIZE);
            dsps_bit_rev_fc32(fft_input, FFT_SIZE);

            // Buscar frecuencia dominante
            float max_mag = 0;
            int max_idx = 0;

            for (int i = 5; i < FFT_SIZE/2; i++) {
                float re = fft_input[2*i];
                float im = fft_input[2*i+1];
                float mag = re*re + im*im;

                if (mag > max_mag) {
                    max_mag = mag;
                    max_idx = i;
                }
            }

            float freq = (float)max_idx * SAMPLE_RATE / FFT_SIZE;

            ESP_LOGI(TAG, "Freq: %.1f Hz | Power: %.0f", freq, max_mag);

            // Decisión
            if (max_mag > 100000.0f) {
                if (fabs(freq - FREQ_FORWARD) < FREQ_TOL) move_robot("FORWARD");
                else if (fabs(freq - FREQ_BACKWARD) < FREQ_TOL) move_robot("BACKWARD");
                else if (fabs(freq - FREQ_LEFT) < FREQ_TOL) move_robot("LEFT");
                else if (fabs(freq - FREQ_RIGHT) < FREQ_TOL) move_robot("RIGHT");
                else move_robot("STOP");
            } else {
                move_robot("STOP");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}