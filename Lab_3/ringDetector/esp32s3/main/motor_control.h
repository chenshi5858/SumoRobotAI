#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct {
    float x_m;
    float y_m;
    float theta_rad;
    float v_mps;
    float omega_rps;
    float v_left_mps;
    float v_right_mps;
    uint8_t pwm_left;
    uint8_t pwm_right;
    int8_t dir_left;
    int8_t dir_right;
} odom_state_t;

esp_err_t motor_control_init(void);

void move_robot(const char *cmd);
void move_forward(void);
void rotate_fast(void);
void stop_motors(void);

void motor_set_speed(uint8_t speed);
void motor_speed_up(void);
void motor_speed_down(void);
uint8_t motor_get_speed(void);

void odom_reset(void);
void odom_snapshot(odom_state_t *out);
void odom_update(float dt_s);
