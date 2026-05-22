#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_camera.h"

esp_err_t init_camera(void);
bool detect_white_region(const camera_fb_t *frame, uint32_t *white_pixels, uint32_t *total_pixels);
