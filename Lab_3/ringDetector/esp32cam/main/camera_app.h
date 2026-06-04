#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_camera.h"

typedef struct {
    uint32_t total_pixels;
    uint32_t edge_pixels;
    uint32_t edge_rows;
    uint32_t best_row_pixels;
    uint16_t edge_threshold;
    bool edge_trigger;
} ring_detection_metrics_t;

esp_err_t init_camera(void);
bool detect_ring_border(const camera_fb_t *frame, ring_detection_metrics_t *metrics);
