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

typedef struct {
    mic_command_t cmd;
    float frequency_hz;
    float magnitude;
    float peak_to_noise_ratio;
} mic_detection_t;

static const char *command_name(mic_command_t cmd) {
    switch (cmd) {
        case MIC_CMD_FULL_FORWARD: return "FULL_FORWARD";
        case MIC_CMD_BACKWARD:     return "BACKWARD";
        default:                   return "NONE";
    }
}

static bool send_command_event(mic_command_t cmd, bool active) {
    if (s_cmd_queue == NULL) {
        return false;
    }

    mic_command_event_t event = {
        .cmd = cmd,
        .active = active,
    };

    if (xQueueSend(s_cmd_queue, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Mic event queue full, dropping cmd=%d active=%d",
                 (int)cmd, active);
        return false;
    }

    ESP_LOGI(TAG, "Mic command %s: %s",
             active ? "started" : "released", command_name(cmd));
    return true;
}

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

static mic_detection_t detect_command(void) {
    mic_detection_t detection = {
        .cmd = MIC_CMD_NONE,
    };
    uint8_t result[MIC_FFT_SIZE * sizeof(adc_digi_output_data_t)];
    uint32_t out_len = 0;

    esp_err_t err = adc_continuous_read(s_adc_handle, result, sizeof(result), &out_len, portMAX_DELAY);
    if (err != ESP_OK || out_len == 0) {
        return detection;
    }

    size_t samples = out_len / sizeof(adc_digi_output_data_t);
    size_t fft_samples = (samples < MIC_FFT_SIZE) ? samples : MIC_FFT_SIZE;
    if (fft_samples == 0) {
        return detection;
    }

    float sample_mean = 0.0f;
    for (size_t i = 0; i < fft_samples; i++) {
        adc_digi_output_data_t *p =
            (adc_digi_output_data_t *)&result[i * sizeof(adc_digi_output_data_t)];
        sample_mean += (float)p->type2.data;
    }
    sample_mean /= fft_samples;

    memset(s_fft_buf, 0, sizeof(s_fft_buf));
    for (size_t i = 0; i < fft_samples; i++) {
        adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i * sizeof(adc_digi_output_data_t)];
        float val = (float)p->type2.data - sample_mean;
        s_fft_buf[i * 2] = val * s_window[i];
        s_fft_buf[i * 2 + 1] = 0.0f;
    }

    dsps_fft2r_fc32(s_fft_buf, fft_samples);
    dsps_bit_rev_fc32(s_fft_buf, fft_samples);

    float max_mag = 0;
    float total_mag = 0;
    int max_idx = 0;
    int analyzed_bins = 0;
    for (int i = 5; i < fft_samples / 2; i++) {
        float re = s_fft_buf[i * 2];
        float im = s_fft_buf[i * 2 + 1];
        float mag = re * re + im * im;
        total_mag += mag;
        analyzed_bins++;
        if (mag > max_mag) {
            max_mag = mag;
            max_idx = i;
        }
    }

    if (analyzed_bins <= 1 || max_mag < MIC_MAG_THRESHOLD) {
        return detection;
    }

    const float noise_floor = (total_mag - max_mag) / (analyzed_bins - 1);
    if (noise_floor > 0.0f && max_mag < noise_floor * MIC_PEAK_TO_NOISE_RATIO) {
        return detection;
    }

    detection.frequency_hz = (float)max_idx * MIC_SAMPLE_RATE / fft_samples;
    detection.magnitude = max_mag;
    detection.peak_to_noise_ratio =
        (noise_floor > 0.0f) ? max_mag / noise_floor : 0.0f;
    detection.cmd = match_freq(detection.frequency_hz, max_mag);
    return detection;
}

static void mic_task(void *arg) {
    (void)arg;
    generate_hann_window();

    mic_command_t candidate = MIC_CMD_NONE;
    mic_command_t active_cmd = MIC_CMD_NONE;
    uint8_t confirm_count = 0;
    uint8_t release_count = 0;

    while (1) {
        mic_detection_t detection = detect_command();
        mic_command_t cmd = detection.cmd;

        if (active_cmd != MIC_CMD_NONE) {
            if (cmd == MIC_CMD_NONE) {
                if (++release_count >= MIC_RELEASE_FRAMES) {
                    if (send_command_event(active_cmd, false)) {
                        active_cmd = MIC_CMD_NONE;
                        release_count = 0;
                    } else {
                        release_count = MIC_RELEASE_FRAMES;
                    }
                }
                candidate = MIC_CMD_NONE;
                confirm_count = 0;
            } else if (cmd == active_cmd) {
                candidate = MIC_CMD_NONE;
                confirm_count = 0;
                release_count = 0;
            } else {
                release_count = 0;
                if (cmd == candidate) {
                    confirm_count++;
                } else {
                    candidate = cmd;
                    confirm_count = 1;
                }

                if (confirm_count >= MIC_CONFIRM_FRAMES) {
                    mic_command_t previous_cmd = active_cmd;
                    if (send_command_event(previous_cmd, false)) {
                        if (send_command_event(cmd, true)) {
                            active_cmd = cmd;
                            ESP_LOGI(TAG,
                                     "Mic command accepted: %s freq=%.1fHz mag=%.0f ratio=%.1f",
                                     command_name(cmd),
                                     detection.frequency_hz,
                                     detection.magnitude,
                                     detection.peak_to_noise_ratio);
                        } else {
                            active_cmd = MIC_CMD_NONE;
                        }
                    }
                    candidate = MIC_CMD_NONE;
                    confirm_count = 0;
                }
            }
        } else if (cmd == MIC_CMD_NONE) {
            candidate = MIC_CMD_NONE;
            confirm_count = 0;
        } else {
            if (cmd == candidate) {
                confirm_count++;
            } else {
                candidate = cmd;
                confirm_count = 1;
            }

            if (confirm_count >= MIC_CONFIRM_FRAMES) {
                if (send_command_event(cmd, true)) {
                    ESP_LOGI(TAG,
                             "Mic command accepted: %s freq=%.1fHz mag=%.0f ratio=%.1f",
                             command_name(cmd),
                             detection.frequency_hz,
                             detection.magnitude,
                             detection.peak_to_noise_ratio);
                    active_cmd = cmd;
                }
                candidate = MIC_CMD_NONE;
                confirm_count = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
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
