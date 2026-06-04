#pragma once

/*
 * Receptor ESP-NOW:
 * - Si ACCEPT_ANY_CAMERA_MAC es 1, se aceptan mensajes de cualquier camara.
 * - Si es 0, se filtra por CAMERA_MAC_BYTES.
 */
#define ACCEPT_ANY_CAMERA_MAC 1
#define CAMERA_MAC_BYTES {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

/* Debe coincidir con ESPNOW_CHANNEL de la ESP32-CAM. */
#define ESPNOW_CHANNEL 6

/* Motor pins heredados del proyecto Lab_3/4. Ajustar segun cableado real. */
#define MOTOR_A_PIN1 6
#define MOTOR_A_PIN2 7
#define MOTOR_B_PIN1 16
#define MOTOR_B_PIN2 17
#define MOTOR_A_ENA 9
#define MOTOR_B_ENB 10

/* Geometria del robot para odometria abierta. */
#define WHEEL_RADIUS_M 0.034f
#define WHEEL_BASE_M 0.13f
#define ANGULAR_SCALE 0.5f

/* PWM y modelo aproximado de motor. */
#define DEFAULT_SPEED 175
#define MAX_SPEED 190
#define MIN_SPEED 80
#define SPEED_STEP 20
#define ROTATE_SPEED 220
#define DIAGONAL_INNER_SPEED_PERCENT 70
#define MOTOR_MAX_RPM 250.0f

/* Mapeo de orientacion de motores. */
#define MOTOR_A_FORWARD_SIGN 1
#define MOTOR_B_FORWARD_SIGN -1
#define MOTOR_A_IS_LEFT 1

/* Tareas periodicas. */
#define ODOM_UPDATE_MS 50
#define AVOID_ROTATE_MS 350
#define ESPNOW_STATUS_TIMEOUT_MS 1500
