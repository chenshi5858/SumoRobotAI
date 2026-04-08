#include "spectrum_export.h"

#include <stdbool.h>
#include <stdio.h>

#include "app_config.h"

#define CSV_CONFIG_TAG "SPECTRUM_CFG"
#define CSV_FRAME_TAG  "SPECTRUM_FRAME"

static bool s_config_printed = false;

void spectrum_export_write_csv(const spectrum_result_t *result, int64_t timestamp_us)
{
    if (!s_config_printed) {
        printf(
            "%s,%d,%d,%zu,%.6f,%s\n",
            CSV_CONFIG_TAG,
            SAMPLE_RATE,
            FFT_SIZE,
            result->bins,
            (double)SAMPLE_RATE / (double)FFT_SIZE,
            result->window_name
        );
        s_config_printed = true;
    }

    printf(
        "%s,%.6f,%.6f,%.6f,%s,%zu",
        CSV_FRAME_TAG,
        (double)timestamp_us / 1000000.0,
        result->peak_frequency_hz,
        result->peak_power_db,
        result->window_name,
        result->bins
    );

    for (size_t i = 0; i < result->bins; ++i) {
        printf(",%.6f", result->power_db[i]);
    }

    printf("\n");
    fflush(stdout);
}
