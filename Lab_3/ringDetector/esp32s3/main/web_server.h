#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "motor_control.h"

esp_err_t web_server_start(void);
void web_server_send_pose(const odom_state_t *pose, int64_t now_ms);
