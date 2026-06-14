#pragma once

#include "esp_err.h"

esp_err_t uart_init(void);
esp_err_t uart_send_beacon_result(uint8_t class_id, uint8_t confidence);
