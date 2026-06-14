#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t coordinator_start(QueueHandle_t cam_queue, QueueHandle_t mic_cmd_queue);
