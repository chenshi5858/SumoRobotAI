#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    const float *power_db;
    size_t bins;
    float peak_frequency_hz;
    float peak_power_db;
    const char *window_name;
} spectrum_result_t;

esp_err_t spectrum_analyzer_init(void);
void spectrum_analyzer_process(const uint8_t *raw_frame, spectrum_result_t *result);
