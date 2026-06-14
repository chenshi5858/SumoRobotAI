#pragma once

#include <stdint.h>

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#define CAMERA_FRAME_WIDTH 96
#define CAMERA_FRAME_HEIGHT 96

#define UART_TX_GPIO 4
#define UART_BAUD_RATE 115200

#define EDGE_ROI_Y_START 0
#define EDGE_ROI_Y_END 96
#define EDGE_ROI_X_START 24
#define EDGE_ROI_X_END 72

#define EDGE_DARK_THRESHOLD 95

#define EDGE_MIN_PERCENT 7
#define EDGE_MIN_ROW_PIXELS 6
#define EDGE_MIN_ACTIVE_ROWS 4

#define DEBOUNCE_COUNT 1

#define CAPTURE_FPS 18
#define CAPTURE_INTERVAL_MS (1000 / CAPTURE_FPS)
