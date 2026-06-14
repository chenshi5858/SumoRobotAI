#pragma once

#include <stdint.h>

#define ESPNOW_PROTOCOL_VERSION 1
#define ESPNOW_MAX_DATA_LEN 250

#define CAMERA_ID_FLOOR   1
#define CAMERA_ID_BEACON  2

#define DATA_TYPE_FLOOR   0
#define DATA_TYPE_BEACON  1
#define DATA_TYPE_MIC     2

#define FLOOR_SAFE  0
#define FLOOR_EDGE  1

#define BEACON_ABSENT  0
#define BEACON_LEFT    1
#define BEACON_CENTER  2
#define BEACON_RIGHT   3

#define MICROPHONE_CMD_FULL_FORWARD  0
#define MICROPHONE_CMD_STOP          1
#define MICROPHONE_CMD_ROTATE_LEFT   2
#define MICROPHONE_CMD_ROTATE_RIGHT  3

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t camera_id;
    uint8_t data_type;
    uint8_t value;
    uint8_t confidence;
    uint8_t reserved[3];
} espnow_camera_message_t;

#define ESPNOW_CAMERA_MSG_SIZE sizeof(espnow_camera_message_t)

_Static_assert(ESPNOW_CAMERA_MSG_SIZE <= ESPNOW_MAX_DATA_LEN,
               "espnow_camera_message_t exceeds ESP-NOW max data length");
