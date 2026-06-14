#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "mic_freq_cmds.h"

esp_err_t microphone_init(QueueHandle_t cmd_queue);
void microphone_start_task(void);
