#include "app_config.h"
#include "audio_sampler.h"
#include "spectrum_analyzer.h"
#include "spectrum_console.h"
#include "spectrum_export.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ANALISIS_ESPECTRO";

static void analysis_task(void *arg)
{
    int64_t last_plot_us = 0;

    ESP_LOGI(TAG, "Inicializando analisis de espectro");
    ESP_LOGI(
        TAG,
        "fs=%d Hz, FFT=%d, periodo=%d ms, export CSV por serial",
        SAMPLE_RATE,
        FFT_SIZE,
        PLOT_INTERVAL_MS
    );

    ESP_ERROR_CHECK(spectrum_analyzer_init());
    ESP_ERROR_CHECK(audio_sampler_init());

    while (1) {
        const uint8_t *frame = NULL;
        uint32_t bytes_read = 0;
        esp_err_t err = audio_sampler_read_frame(&frame, &bytes_read, ADC_READ_TIMEOUT_MS);

        if (err == ESP_ERR_TIMEOUT) {
            continue;
        }

        if (err == ESP_ERR_INVALID_SIZE) {
            continue;
        }

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Error leyendo ADC: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        spectrum_result_t result = {0};
        spectrum_analyzer_process(frame, &result);

        int64_t now_us = esp_timer_get_time();
        if ((now_us - last_plot_us) >= (int64_t)PLOT_INTERVAL_MS * 1000) {
#if ENABLE_ASCII_PLOT
            spectrum_console_plot(&result, now_us);
#endif
            spectrum_export_write_csv(&result, now_us);
            ESP_LOGI(
                TAG,
                "Frame exportado | t=%.2f s | pico=%.1f Hz | %.1f dB",
                (double)now_us / 1000000.0,
                result.peak_frequency_hz,
                result.peak_power_db
            );
            last_plot_us = now_us;
        }
    }
}

void app_main(void)
{
    BaseType_t ok = xTaskCreatePinnedToCore(
        analysis_task,
        "analysis_task",
        ANALYSIS_TASK_STACK_SIZE,
        NULL,
        ANALYSIS_TASK_PRIORITY,
        NULL,
        tskNO_AFFINITY
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "No se pudo crear la tarea de analisis");
    }
}
