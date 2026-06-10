#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_camera.h"

typedef struct {
    uint32_t total_pixels;
    uint32_t black_pixels;
    uint32_t black_percent;
    bool edge_detected;
} edge_metrics_t;

esp_err_t init_camera(void);
bool detect_edge(const camera_fb_t *frame, edge_metrics_t *metrics);
