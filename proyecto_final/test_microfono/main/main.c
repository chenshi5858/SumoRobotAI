#include <stdio.h>
#include <string.h>

#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "esp_adc/adc_continuous.h"
#include "esp_dsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mic_freq_cmds.h"

static const char *TAG = "mic_test";

#define MIC_ADC_CHANNEL ADC_CHANNEL_3
#define SAMPLE_RATE     16000
#define FFT_SIZE         1024

static adc_continuous_handle_t s_adc_handle = NULL;
static float s_fft_buf[FFT_SIZE * 2];
static float s_window[FFT_SIZE];

static void init_adc(void) {
    adc_continuous_handle_cfg_t cfg = {
        .max_store_buf_size = FFT_SIZE * sizeof(adc_digi_output_data_t) * 2,
        .conv_frame_size = FFT_SIZE * sizeof(adc_digi_output_data_t),
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&cfg, &s_adc_handle));

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

    ESP_ERROR_CHECK(adc_continuous_config(s_adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(s_adc_handle));

    ESP_LOGI(TAG, "ADC ready: GPIO4, rate=%d, FFT=%d", SAMPLE_RATE, FFT_SIZE);
}

static const char *cmd_name(mic_command_t cmd) {
    switch (cmd) {
        case MIC_CMD_FULL_FORWARD: return "FULL_FORWARD";
        case MIC_CMD_BACKWARD:     return "BACKWARD";
        default:                   return "NONE";
    }
}

static void analyze_task(void *arg) {
    (void)arg;

    dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    dsps_wind_hann_f32(s_window, FFT_SIZE);

    ESP_LOGI(TAG, "=== Mic test (Lab_1 style: dominant freq, fixed threshold) ===");
    for (int i = 0; i < FREQ_CMD_TABLE_COUNT; i++) {
        ESP_LOGI(TAG, "  [%d] %.0f Hz -> %s",
                 i, FREQ_CMD_TABLE[i].freq_hz, cmd_name(FREQ_CMD_TABLE[i].cmd));
    }

    uint8_t result[FFT_SIZE * sizeof(adc_digi_output_data_t)];
    uint32_t out_len = 0;

    while (1) {
        esp_err_t err = adc_continuous_read(s_adc_handle, result, sizeof(result), &out_len, portMAX_DELAY);
        if (err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t samples = out_len / sizeof(adc_digi_output_data_t);
        size_t fft_samples = (samples < FFT_SIZE) ? samples : FFT_SIZE;

        memset(s_fft_buf, 0, sizeof(s_fft_buf));
        for (size_t i = 0; i < fft_samples; i++) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i * sizeof(adc_digi_output_data_t)];
            float val = (float)p->type2.data - 2048.0f;
            s_fft_buf[i * 2] = val * s_window[i];
            s_fft_buf[i * 2 + 1] = 0.0f;
        }

        dsps_fft2r_fc32(s_fft_buf, fft_samples);
        dsps_bit_rev_fc32(s_fft_buf, fft_samples);

        float max_mag = 0;
        int max_idx = 0;
        for (int i = 5; i < fft_samples / 2; i++) {
            float re = s_fft_buf[i * 2];
            float im = s_fft_buf[i * 2 + 1];
            float mag = re * re + im * im;
            if (mag > max_mag) {
                max_mag = mag;
                max_idx = i;
            }
        }

        float freq = (float)max_idx * SAMPLE_RATE / fft_samples;
        mic_command_t cmd = match_freq(freq, max_mag);

        static int64_t last_print = 0;
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_print >= 600) {
            last_print = now;
            ESP_LOGI(TAG, "[DOM] %.0f Hz (mag=%.0f) | CMD: %s",
                     freq, max_mag, cmd_name(cmd));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "TEST MICROFONO (Lab_1 style)");
    init_adc();
    xTaskCreate(analyze_task, "analyze", 8192, NULL, 5, NULL);
}
