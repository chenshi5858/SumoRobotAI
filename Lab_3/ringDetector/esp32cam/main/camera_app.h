#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_camera.h"

typedef struct {
    uint32_t total_pixels;
    uint32_t black_pixels;
    uint32_t black_percent;
    bool black_detected;
} black_line_metrics_t;

esp_err_t init_camera(void);
bool detect_black_line(const camera_fb_t *frame, black_line_metrics_t *metrics);
