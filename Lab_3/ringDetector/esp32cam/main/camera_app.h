#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_camera.h"

typedef struct {
    uint32_t total_pixels;
    uint32_t edge_pixels;
    uint32_t edge_percent;
    uint32_t active_rows;
    uint32_t best_row_edges;
    bool edge_detected;
} edge_metrics_t;

esp_err_t init_camera(void);
bool detect_edge(const camera_fb_t *frame, edge_metrics_t *metrics);
void debug_print_edge_map(void);
