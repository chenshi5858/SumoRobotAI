#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    char status[16];
    uint8_t source_mac[6];
    int64_t rx_time_ms;
} espnow_status_msg_t;

esp_err_t espnow_receiver_init(QueueHandle_t status_queue);
