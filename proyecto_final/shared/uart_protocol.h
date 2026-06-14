#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "espnow_protocol.h"

#define UART_MSG_START_BYTE 0xAA
#define UART_FRAME_SIZE (1 + ESPNOW_CAMERA_MSG_SIZE)

static inline size_t uart_frame_prepare(const espnow_camera_message_t *msg, uint8_t *frame) {
    frame[0] = UART_MSG_START_BYTE;
    memcpy(frame + 1, msg, ESPNOW_CAMERA_MSG_SIZE);
    return UART_FRAME_SIZE;
}

static inline bool uart_frame_parse(const uint8_t *buf, size_t len, espnow_camera_message_t *msg) {
    if (len < UART_FRAME_SIZE) {
        return false;
    }
    if (buf[0] != UART_MSG_START_BYTE) {
        return false;
    }
    memcpy(msg, buf + 1, ESPNOW_CAMERA_MSG_SIZE);
    return msg->version == ESPNOW_PROTOCOL_VERSION;
}
