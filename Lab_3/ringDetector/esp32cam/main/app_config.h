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

/* La radio de ambos equipos debe usar el mismo canal ESP-NOW. */
#define ESPNOW_CHANNEL 1

/*
 * Por defecto se envia a broadcast para facilitar la primera prueba.
 * Para produccion, reemplazar por la MAC STA del ESP32-S3.
 */
#define ESPNOW_PEER_MAC_BYTES {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/* Deteccion de blanco en el borde inferior de la imagen. */
#define WHITE_PIXEL_THRESHOLD 200
#define WHITE_REGION_PERCENT 25
#define WHITE_MIN_PERCENT 15

/* 50 ms equivale a unas 20 muestras por segundo. */
#define CAPTURE_INTERVAL_MS 50
