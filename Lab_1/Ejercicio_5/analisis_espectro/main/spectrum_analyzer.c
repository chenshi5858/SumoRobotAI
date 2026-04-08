#include "spectrum_analyzer.h"

#include <math.h>

#include "app_config.h"
#include "esp_adc/adc_continuous.h"
#include "esp_dsp.h"

#if USE_HANN_WINDOW
#define WINDOW_NAME "hann"
#else
#define WINDOW_NAME "rectangular"
#endif

__attribute__((aligned(16))) static float s_fft_buffer[FFT_SIZE * 2];
__attribute__((aligned(16))) static float s_window[FFT_SIZE];
static float s_power_db[FFT_SIZE / 2];

static float sample_to_float(const uint8_t *raw_frame, int index)
{
    const adc_digi_output_data_t *sample =
        (const adc_digi_output_data_t *)&raw_frame[index * sizeof(adc_digi_output_data_t)];
    return (float)sample->type2.data;
}

static void init_window(void)
{
#if USE_HANN_WINDOW
    dsps_wind_hann_f32(s_window, FFT_SIZE);
#else
    for (int i = 0; i < FFT_SIZE; ++i) {
        s_window[i] = 1.0f;
    }
#endif
}

esp_err_t spectrum_analyzer_init(void)
{
    esp_err_t err = dsps_fft2r_init_fc32(NULL, FFT_SIZE);
    if (err != ESP_OK) {
        return err;
    }

    init_window();
    return ESP_OK;
}

void spectrum_analyzer_process(const uint8_t *raw_frame, spectrum_result_t *result)
{
    int peak_index = MIN_PEAK_BIN;
    float peak_power_db = -INFINITY;
    float mean = 0.0f;

    for (int i = 0; i < FFT_SIZE; ++i) {
        mean += sample_to_float(raw_frame, i);
    }
    mean /= FFT_SIZE;

    for (int i = 0; i < FFT_SIZE; ++i) {
        float centered = sample_to_float(raw_frame, i) - mean;
        s_fft_buffer[2 * i] = centered * s_window[i];
        s_fft_buffer[2 * i + 1] = 0.0f;
    }

    dsps_fft2r_fc32(s_fft_buffer, FFT_SIZE);
    dsps_bit_rev_fc32(s_fft_buffer, FFT_SIZE);
    dsps_cplx2reC_fc32(s_fft_buffer, FFT_SIZE);

    for (int i = 0; i < FFT_SIZE / 2; ++i) {
        float real = s_fft_buffer[2 * i];
        float imag = s_fft_buffer[2 * i + 1];
        float power_linear = (real * real + imag * imag) / FFT_SIZE;
        float power_db = 10.0f * log10f(power_linear + 1e-12f);

        s_power_db[i] = power_db;

        if (i >= MIN_PEAK_BIN && power_db > peak_power_db) {
            peak_power_db = power_db;
            peak_index = i;
        }
    }

    result->power_db = s_power_db;
    result->bins = FFT_SIZE / 2;
    result->peak_frequency_hz = ((float)peak_index * SAMPLE_RATE) / FFT_SIZE;
    result->peak_power_db = peak_power_db;
    result->window_name = WINDOW_NAME;
}
