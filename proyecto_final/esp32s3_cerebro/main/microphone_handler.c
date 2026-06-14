#include "microphone_handler.h"

#include <string.h>

#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "esp_adc/adc_continuous.h"
#include "esp_dsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "main_config.h"
#include "mic_freq_cmds.h"

static const char *TAG = "microphone";

static QueueHandle_t s_cmd_queue = NULL;
static adc_continuous_handle_t s_adc_handle = NULL;
static float s_fft_buf[MIC_FFT_SIZE * 2];
static float s_window[MIC_FFT_SIZE];

static void generate_hann_window(void) {
    dsps_wind_hann_f32(s_window, MIC_FFT_SIZE);
}

static esp_err_t init_adc_microphone(void) {
    adc_continuous_handle_cfg_t cfg = {
        .max_store_buf_size = MIC_FFT_SIZE * sizeof(adc_digi_output_data_t) * 2,
        .conv_frame_size = MIC_FFT_SIZE * sizeof(adc_digi_output_data_t),
    };

    esp_err_t err = adc_continuous_new_handle(&cfg, &s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC handle create failed: %s", esp_err_to_name(err));
        return err;
    }

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = MIC_SAMPLE_RATE,
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

    err = adc_continuous_config(s_adc_handle, &dig_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = adc_continuous_start(s_adc_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ADC microphone ready: GPIO%d, rate=%d, FFT=%d",
             MIC_ADC_GPIO, MIC_SAMPLE_RATE, MIC_FFT_SIZE);
    return ESP_OK;
}

static mic_command_t detect_command(void) {
    uint8_t result[MIC_FFT_SIZE * sizeof(adc_digi_output_data_t)];
    uint32_t out_len = 0;

    esp_err_t err = adc_continuous_read(s_adc_handle, result, sizeof(result), &out_len, portMAX_DELAY);
    if (err != ESP_OK || out_len == 0) {
        return MIC_CMD_NONE;
    }

    size_t samples = out_len / sizeof(adc_digi_output_data_t);
    size_t fft_samples = (samples < MIC_FFT_SIZE) ? samples : MIC_FFT_SIZE;

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

    float freq = (float)max_idx * MIC_SAMPLE_RATE / fft_samples;
    return match_freq(freq, max_mag);
}

static void mic_task(void *arg) {
    (void)arg;
    generate_hann_window();

    while (1) {
        mic_command_t cmd = detect_command();

        if (cmd != MIC_CMD_NONE && s_cmd_queue != NULL) {
            if (xQueueSend(s_cmd_queue, &cmd, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Mic cmd queue full, dropping cmd=%d", (int)cmd);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t microphone_init(QueueHandle_t cmd_queue) {
    if (cmd_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_cmd_queue = cmd_queue;

    esp_err_t err = init_adc_microphone();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADC microphone init failed");
        return err;
    }

    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "FFT init warning: %d", ret);
    }

    ESP_LOGI(TAG, "Microphone handler ready");
    return ESP_OK;
}

void microphone_start_task(void) {
    xTaskCreate(mic_task, "mic_task", 8192, NULL, 4, NULL);
    ESP_LOGI(TAG, "Microphone task started");
}
