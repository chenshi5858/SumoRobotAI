#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "espnow_protocol.h"

typedef struct {
    espnow_camera_message_t msg;
    uint8_t source_mac[6];
    int64_t rx_time_ms;
} camera_msg_t;

typedef struct {
    uart_port_t uart_num;
    int rx_gpio;
    uint8_t camera_id;
} uart_cam_config_t;

esp_err_t uart_receiver_start(const uart_cam_config_t *config, QueueHandle_t out_queue);
