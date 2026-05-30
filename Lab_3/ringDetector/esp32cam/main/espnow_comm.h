#pragma once

#include "esp_err.h"

esp_err_t init_espnow(void);
esp_err_t send_status(const char *status);
