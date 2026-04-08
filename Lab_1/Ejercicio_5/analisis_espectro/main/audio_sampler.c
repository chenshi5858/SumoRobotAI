#include "audio_sampler.h"

#include "app_config.h"
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"

#define MIC_ADC_CHANNEL ADC_CHANNEL_1

static adc_continuous_handle_t s_adc_handle = NULL;
static uint8_t s_adc_frame[FFT_SIZE * sizeof(adc_digi_output_data_t)];

esp_err_t audio_sampler_init(void)
{
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = FFT_SIZE * sizeof(adc_digi_output_data_t) * 2,
        .conv_frame_size = FFT_SIZE * sizeof(adc_digi_output_data_t),
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &s_adc_handle));

    adc_digi_pattern_config_t pattern = {
        .atten = ADC_ATTEN_DB_12,
        .channel = MIC_ADC_CHANNEL,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    adc_continuous_config_t adc_config = {
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
        .pattern_num = 1,
        .adc_pattern = &pattern,
    };

    ESP_ERROR_CHECK(adc_continuous_config(s_adc_handle, &adc_config));
    ESP_ERROR_CHECK(adc_continuous_start(s_adc_handle));

    return ESP_OK;
}

esp_err_t audio_sampler_read_frame(const uint8_t **frame_out, uint32_t *bytes_read_out, uint32_t timeout_ms)
{
    uint32_t bytes_read = 0;
    esp_err_t err = adc_continuous_read(
        s_adc_handle,
        s_adc_frame,
        sizeof(s_adc_frame),
        &bytes_read,
        pdMS_TO_TICKS(timeout_ms)
    );

    if (err != ESP_OK) {
        return err;
    }

    if (bytes_read < sizeof(s_adc_frame)) {
        return ESP_ERR_INVALID_SIZE;
    }

    *frame_out = s_adc_frame;
    *bytes_read_out = bytes_read;
    return ESP_OK;
}
