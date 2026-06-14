#pragma once

#include "esp_err.h"

esp_err_t uart_init(void);
esp_err_t uart_send_floor_status(uint8_t value);
