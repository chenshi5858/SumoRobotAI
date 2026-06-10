#pragma once

#include <stdint.h>

/*
 * ESP32-CAM AI Thinker pinout.
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
#define ESPNOW_CHANNEL 6

/*
 * MAC del receptor ESP32-S3 (STA).
 * Nota: se usa broadcast por defecto.
 */
#define ESPNOW_PEER_MAC_BYTES {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/*
 * Detector de borde NEGRO por umbral de brillo.
 * Cuenta pixeles cuyo brillo promedio (R+G+B)/3 <= BLACK_THRESHOLD.
 * Si el porcentaje de pixeles negros en el frame >= BLACK_MIN_PERCENT,
 * se declara BORDE (envia "1").
 *
 * A diferencia del Sobel Gy que detecta transiciones, este metodo
 * detecta areas solidas de color negro (la cinta del borde).
 */
#define BLACK_THRESHOLD    75   /* brillo <= esto cuenta como negro */
#define BLACK_MIN_PERCENT  4    /* % minimo de negro para declarar borde */

#define CAPTURE_INTERVAL_MS 200
