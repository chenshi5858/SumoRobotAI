#include "motor_control.h"

#include <math.h>
#include <stdbool.h>
#include <strings.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "main_config.h"
#include "robot_state.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "motor_control";

static odom_state_t s_odom = {0};
static portMUX_TYPE s_odom_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE s_speed_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_current_speed = DEFAULT_SPEED;

static float wrap_angle(float angle) {
    while (angle > (float)M_PI) {
        angle -= 2.0f * (float)M_PI;
    }
    while (angle < -(float)M_PI) {
        angle += 2.0f * (float)M_PI;
    }
    return angle;
}

static float pwm_to_rps(uint8_t pwm) {
    if (pwm < MIN_SPEED) {
        return 0.0f;
    }

    const float normalized = (float)pwm / 255.0f;
    const float rpm = normalized * MOTOR_MAX_RPM;
    return rpm / 60.0f;
}

static void odom_set_command(int8_t left_dir, uint8_t left_pwm, int8_t right_dir, uint8_t right_pwm) {
    portENTER_CRITICAL(&s_odom_lock);
    s_odom.dir_left = left_dir;
    s_odom.dir_right = right_dir;
    s_odom.pwm_left = left_pwm;
    s_odom.pwm_right = right_pwm;
    portEXIT_CRITICAL(&s_odom_lock);
}

void odom_reset(void) {
    portENTER_CRITICAL(&s_odom_lock);
    s_odom.x_m = 0.0f;
    s_odom.y_m = 0.0f;
    s_odom.theta_rad = 0.0f;
    s_odom.v_mps = 0.0f;
    s_odom.omega_rps = 0.0f;
    s_odom.v_left_mps = 0.0f;
    s_odom.v_right_mps = 0.0f;
    portEXIT_CRITICAL(&s_odom_lock);
}

void odom_snapshot(odom_state_t *out) {
    if (out == NULL) {
        return;
    }

    portENTER_CRITICAL(&s_odom_lock);
    *out = s_odom;
    portEXIT_CRITICAL(&s_odom_lock);
}

void odom_update(float dt_s) {
    if (dt_s <= 0.0f) {
        return;
    }
    if (dt_s > 0.2f) {
        dt_s = 0.2f;
    }

    int8_t left_dir = 0;
    int8_t right_dir = 0;
    uint8_t left_pwm = 0;
    uint8_t right_pwm = 0;
    float theta = 0.0f;

    portENTER_CRITICAL(&s_odom_lock);
    left_dir = s_odom.dir_left;
    right_dir = s_odom.dir_right;
    left_pwm = s_odom.pwm_left;
    right_pwm = s_odom.pwm_right;
    theta = s_odom.theta_rad;
    portEXIT_CRITICAL(&s_odom_lock);

    const float left_rps = pwm_to_rps(left_pwm);
    const float right_rps = pwm_to_rps(right_pwm);
    const float v_left = (float)left_dir * left_rps * 2.0f * (float)M_PI * WHEEL_RADIUS_M;
    const float v_right = (float)right_dir * right_rps * 2.0f * (float)M_PI * WHEEL_RADIUS_M;
    const float v = 0.5f * (v_left + v_right);
    const float omega_ccw = (v_right - v_left) / WHEEL_BASE_M;

    const float omega = -omega_ccw * ANGULAR_SCALE;
    theta = wrap_angle(theta + omega * dt_s);

    const float dx = v * sinf(theta) * dt_s;
    const float dy = v * cosf(theta) * dt_s;

    portENTER_CRITICAL(&s_odom_lock);
    s_odom.x_m += dx;
    s_odom.y_m += dy;
    s_odom.theta_rad = theta;
    s_odom.v_mps = v;
    s_odom.omega_rps = omega;
    s_odom.v_left_mps = v_left;
    s_odom.v_right_mps = v_right;
    portEXIT_CRITICAL(&s_odom_lock);
}

static void set_motor_dir_pins(int pin1, int pin2, int8_t dir) {
    if (dir > 0) {
        gpio_set_level(pin1, 1);
        gpio_set_level(pin2, 0);
    } else if (dir < 0) {
        gpio_set_level(pin1, 0);
        gpio_set_level(pin2, 1);
    } else {
        gpio_set_level(pin1, 0);
        gpio_set_level(pin2, 0);
    }
}

static esp_err_t init_pwm(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        return err;
    }

    ledc_channel_config_t ledc_channel_a = {
        .gpio_num = MOTOR_A_ENA,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    err = ledc_channel_config(&ledc_channel_a);
    if (err != ESP_OK) {
        return err;
    }

    ledc_channel_config_t ledc_channel_b = {
        .gpio_num = MOTOR_B_ENB,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    return ledc_channel_config(&ledc_channel_b);
}

esp_err_t motor_control_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << MOTOR_A_PIN1) |
            (1ULL << MOTOR_A_PIN2) |
            (1ULL << MOTOR_B_PIN1) |
            (1ULL << MOTOR_B_PIN2),
        .mode = GPIO_MODE_OUTPUT,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO motor init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = init_pwm();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PWM init failed: %s", esp_err_to_name(err));
        return err;
    }

    stop_motors();
    ESP_LOGI(TAG, "Motors ready");
    return ESP_OK;
}

static void set_motor_speeds(uint8_t motor_a_speed, uint8_t motor_b_speed) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, motor_a_speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, motor_b_speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

uint8_t motor_get_speed(void) {
    uint8_t speed = 0;

    portENTER_CRITICAL(&s_speed_lock);
    speed = s_current_speed;
    portEXIT_CRITICAL(&s_speed_lock);

    return speed;
}

void motor_set_speed(uint8_t speed) {
    if (speed < MIN_SPEED) {
        speed = MIN_SPEED;
    }

    portENTER_CRITICAL(&s_speed_lock);
    s_current_speed = speed;
    portEXIT_CRITICAL(&s_speed_lock);

    ESP_LOGI(TAG, "Speed set to %u", speed);
}

void motor_speed_up(void) {
    uint16_t speed = motor_get_speed();
    if (speed + SPEED_STEP >= MAX_SPEED) {
        speed = MAX_SPEED;
    } else {
        speed += SPEED_STEP;
    }
    motor_set_speed((uint8_t)speed);
}

void motor_speed_down(void) {
    uint16_t speed = motor_get_speed();
    if (speed <= (uint16_t)MIN_SPEED + SPEED_STEP) {
        speed = MIN_SPEED;
    } else {
        speed -= SPEED_STEP;
    }
    motor_set_speed((uint8_t)speed);
}

static uint8_t diagonal_inner_speed(void) {
    const uint16_t scaled_speed = ((uint16_t)motor_get_speed() * DIAGONAL_INNER_SPEED_PERCENT) / 100;
    if (scaled_speed > 0 && scaled_speed < MIN_SPEED) {
        return MIN_SPEED;
    }
    return (uint8_t)scaled_speed;
}

static uint8_t compensate_motor(uint8_t pwm, uint16_t speed_percent) {
    if (pwm == 0) {
        return 0;
    }

    const uint16_t compensated =
        ((uint16_t)pwm * speed_percent + 50U) / 100U;
    return (uint8_t)(compensated > MAX_SPEED ? MAX_SPEED : compensated);
}

static void apply_wheel_command(int8_t left_dir, uint8_t left_pwm, int8_t right_dir, uint8_t right_pwm) {
    int8_t a_dir = 0;
    int8_t b_dir = 0;
    uint8_t a_pwm = 0;
    uint8_t b_pwm = 0;
    const uint8_t compensated_left_pwm =
        compensate_motor(left_pwm, LEFT_MOTOR_SPEED_PERCENT);
    const uint8_t compensated_right_pwm =
        compensate_motor(right_pwm, RIGHT_MOTOR_SPEED_PERCENT);

    if (MOTOR_A_IS_LEFT) {
        a_dir = left_dir * MOTOR_A_FORWARD_SIGN;
        b_dir = right_dir * MOTOR_B_FORWARD_SIGN;
        a_pwm = compensated_left_pwm;
        b_pwm = compensated_right_pwm;
    } else {
        a_dir = right_dir * MOTOR_A_FORWARD_SIGN;
        b_dir = left_dir * MOTOR_B_FORWARD_SIGN;
        a_pwm = compensated_right_pwm;
        b_pwm = compensated_left_pwm;
    }

    set_motor_dir_pins(MOTOR_A_PIN1, MOTOR_A_PIN2, a_dir);
    set_motor_dir_pins(MOTOR_B_PIN1, MOTOR_B_PIN2, b_dir);
    set_motor_speeds(a_pwm, b_pwm);
    odom_set_command(left_dir, compensated_left_pwm, right_dir, compensated_right_pwm);
}

void move_robot(const char *cmd) {
    if (cmd == NULL) {
        return;
    }

    const uint8_t speed = motor_get_speed();
    const uint8_t inner_speed = diagonal_inner_speed();

    if (strcasecmp(cmd, "forward") == 0 || strcasecmp(cmd, "f") == 0) {
        apply_wheel_command(1, speed, 1, speed);
        robot_state_set_motion("forward");
    } else if (strcasecmp(cmd, "back") == 0 || strcasecmp(cmd, "backward") == 0 || strcasecmp(cmd, "b") == 0) {
        apply_wheel_command(-1, speed, -1, speed);
        robot_state_set_motion("back");
    } else if (strcasecmp(cmd, "forward_right") == 0 || strcasecmp(cmd, "fr") == 0) {
        apply_wheel_command(1, speed, 1, inner_speed);
        robot_state_set_motion("forward_right");
    } else if (strcasecmp(cmd, "forward_left") == 0 || strcasecmp(cmd, "fl") == 0) {
        apply_wheel_command(1, inner_speed, 1, speed);
        robot_state_set_motion("forward_left");
    } else if (strcasecmp(cmd, "back_right") == 0 || strcasecmp(cmd, "br") == 0) {
        apply_wheel_command(-1, speed, -1, inner_speed);
        robot_state_set_motion("back_right");
    } else if (strcasecmp(cmd, "back_left") == 0 || strcasecmp(cmd, "bl") == 0) {
        apply_wheel_command(-1, inner_speed, -1, speed);
        robot_state_set_motion("back_left");
    } else if (strcasecmp(cmd, "left") == 0 || strcasecmp(cmd, "l") == 0) {
        apply_wheel_command(-1, speed, 1, speed);
        robot_state_set_motion("left");
    } else if (strcasecmp(cmd, "right") == 0 || strcasecmp(cmd, "r") == 0) {
        apply_wheel_command(1, speed, -1, speed);
        robot_state_set_motion("right");
    } else {
        apply_wheel_command(0, 0, 0, 0);
        robot_state_set_motion("stop");
    }
}

void move_forward(void) {
    move_robot("forward");
}

void move_forward_full(void) {
    const uint8_t full_speed = MAX_SPEED;
    apply_wheel_command(1, full_speed, 1, full_speed);
    robot_state_set_motion("full_forward");
    ESP_LOGI(TAG, "FULL FORWARD (speed: %u)", full_speed);
}

void rotate_fast(void) {
    apply_wheel_command(-1, ROTATE_SPEED, 1, ROTATE_SPEED);
    robot_state_set_motion("rotate_fast");
}

void rotate_left(void) {
    apply_wheel_command(-1, ROTATE_SPEED, 1, ROTATE_SPEED);
    robot_state_set_motion("rotate_left");
}

void rotate_right(void) {
    apply_wheel_command(1, ROTATE_SPEED, -1, ROTATE_SPEED);
    robot_state_set_motion("rotate_right");
}

void stop_motors(void) {
    move_robot("stop");
}
