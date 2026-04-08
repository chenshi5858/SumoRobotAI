#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t audio_sampler_init(void);
esp_err_t audio_sampler_read_frame(const uint8_t **frame_out, uint32_t *bytes_read_out, uint32_t timeout_ms);
