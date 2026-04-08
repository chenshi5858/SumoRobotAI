#include "spectrum_console.h"

#include <stdio.h>

#include "app_config.h"
#include "esp_dsp.h"

static void compute_display_range(const spectrum_result_t *result, float *min_db, float *max_db)
{
    float local_min = result->power_db[MIN_PEAK_BIN];
    float local_max = result->power_db[MIN_PEAK_BIN];

    for (size_t i = MIN_PEAK_BIN + 1; i < result->bins; ++i) {
        if (result->power_db[i] < local_min) {
            local_min = result->power_db[i];
        }
        if (result->power_db[i] > local_max) {
            local_max = result->power_db[i];
        }
    }

    *min_db = local_min - SPECTRUM_DB_PADDING;
    *max_db = local_max + SPECTRUM_DB_PADDING;

    if ((*max_db - *min_db) < 6.0f) {
        *min_db -= 3.0f;
        *max_db += 3.0f;
    }
}

void spectrum_console_plot(const spectrum_result_t *result, int64_t timestamp_us)
{
    float min_db = 0.0f;
    float max_db = 0.0f;
    compute_display_range(result, &min_db, &max_db);

    printf(
        "\n===== ESPECTRO | t=%.2f s | pico=%.1f Hz | %.1f dB | ventana=%s =====\n",
        (double)timestamp_us / 1000000.0,
        result->peak_frequency_hz,
        result->peak_power_db,
        result->window_name
    );
    printf("Rango mostrado: %.1f dB a %.1f dB | fs=%d Hz | N=%d\n", min_db, max_db, SAMPLE_RATE, FFT_SIZE);

    dsps_view(
        (float *)result->power_db,
        result->bins,
        SPECTRUM_PLOT_WIDTH,
        SPECTRUM_PLOT_HEIGHT,
        min_db,
        max_db,
        '|'
    );

    printf("=====================================================================\n");
    fflush(stdout);
}
