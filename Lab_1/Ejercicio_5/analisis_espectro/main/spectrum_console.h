#pragma once

#include <stdint.h>

#include "spectrum_analyzer.h"

void spectrum_console_plot(const spectrum_result_t *result, int64_t timestamp_us);
