#pragma once

#include <stdint.h>

/*
 * ESP32-CAM AI Thinker pinout.
 * Ajustar aqui si se usa otro modulo de camara.
 */
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

/* La radio de ambos equipos debe usar el mismo canal ESP-NOW (mismo canal del AP). */
#define ESPNOW_CHANNEL 6

/*
 * MAC del receptor ESP32-S3 (STA).
 * Para obtenerla: conecta el S3 por USB y busca en el monitor serial:
 *   "ESP32-S3 STA MAC: XX:XX:XX:XX:XX:XX"
 * Copia esos 6 bytes aqui abajo.
 * Ejemplo: #define ESPNOW_PEER_MAC_BYTES {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
 *
 * NOTA: Se usa broadcast porque unicast requiere ACK del S3 y si el S3
 * esta ocupado la cola TX se llena. Broadcast no espera ACK.
 */
#define ESPNOW_PEER_MAC_BYTES {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/* Deteccion de blanco estilo example (ROI fijo y umbral de luminancia). */
#define WHITE_USE_ADAPTIVE_THRESHOLD 0
#define WHITE_LUMA_THRESHOLD 180
#define WHITE_ROI_Y_START 72
#define WHITE_ROI_Y_END 96
#define WHITE_ROI_X_START 24
#define WHITE_ROI_X_END 72
#define WHITE_MIN_PERCENT 15
#define WHITE_DEBOUNCE_COUNT 1

/* 200 ms equivale a 5 muestras por segundo. */
#define CAPTURE_INTERVAL_MS 200
