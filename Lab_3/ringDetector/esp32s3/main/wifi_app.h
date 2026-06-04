#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t wifi_app_init_sta(void);
uint8_t wifi_app_get_primary_channel(void);
