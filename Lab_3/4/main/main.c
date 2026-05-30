#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "LAB3_EJ4_POSE";

/* WiFi credentials: update these */
#define WIFI_SSID "VTR-8659428"
#define WIFI_PASS "Lala9521"

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;
static httpd_handle_t http_server = NULL;

/* Motor pins (adjust to your wiring) */
#define MOTOR_A_PIN1 6
#define MOTOR_A_PIN2 7
#define MOTOR_B_PIN1 16
#define MOTOR_B_PIN2 17
#define MOTOR_A_ENA  9
#define MOTOR_B_ENB  10

/* Wheel and chassis geometry (meters) */
#define WHEEL_RADIUS_M 0.034f
#define WHEEL_BASE_M   0.13f
/* Scale factor for yaw rate (use <1 if rotation looks too fast) */
#define ANGULAR_SCALE 0.5f

/* PWM settings: locked to max speed */
static uint8_t current_speed = 255;
static const uint8_t MAX_SPEED = 255;
static const uint8_t MIN_SPEED = 255;
static const uint8_t SPEED_STEP = 0;
static const uint8_t DIAGONAL_INNER_SPEED_PERCENT = 100;

/* Motor model (no sensors): PWM -> RPM (adjust for your supply) */
#define MOTOR_MAX_RPM 250.0f
#define PWM_DEADBAND MIN_SPEED

/* Motor orientation mapping (tune to match forward motion) */
#define MOTOR_A_FORWARD_SIGN 1
#define MOTOR_B_FORWARD_SIGN 1
#define MOTOR_A_IS_LEFT 1

/* Odometry update rates */
#define ODOM_UPDATE_MS 50
#define POSE_TX_MS 200

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

static odom_state_t odom = {0};
static portMUX_TYPE odom_lock = portMUX_INITIALIZER_UNLOCKED;

static int ws_client_fd = -1;
static bool ws_client_connected = false;
static portMUX_TYPE ws_lock = portMUX_INITIALIZER_UNLOCKED;

static char last_motion[16] = "stop";

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
    if (pwm < PWM_DEADBAND) {
        return 0.0f;
    }
    const float normalized = (float)pwm / 255.0f;
    const float rpm = normalized * MOTOR_MAX_RPM;
    return rpm / 60.0f;
}

static void odom_set_command(int8_t left_dir, uint8_t left_pwm, int8_t right_dir, uint8_t right_pwm) {
    portENTER_CRITICAL(&odom_lock);
    odom.dir_left = left_dir;
    odom.dir_right = right_dir;
    odom.pwm_left = left_pwm;
    odom.pwm_right = right_pwm;
    portEXIT_CRITICAL(&odom_lock);
}

static void odom_reset(void) {
    portENTER_CRITICAL(&odom_lock);
    odom.x_m = 0.0f;
    odom.y_m = 0.0f;
    odom.theta_rad = 0.0f;
    odom.v_mps = 0.0f;
    odom.omega_rps = 0.0f;
    odom.v_left_mps = 0.0f;
    odom.v_right_mps = 0.0f;
    portEXIT_CRITICAL(&odom_lock);
}

static void odom_snapshot(odom_state_t *out) {
    portENTER_CRITICAL(&odom_lock);
    *out = odom;
    portEXIT_CRITICAL(&odom_lock);
}

static void odom_update(float dt_s) {
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

    portENTER_CRITICAL(&odom_lock);
    left_dir = odom.dir_left;
    right_dir = odom.dir_right;
    left_pwm = odom.pwm_left;
    right_pwm = odom.pwm_right;
    theta = odom.theta_rad;
    portEXIT_CRITICAL(&odom_lock);

    const float left_rps = pwm_to_rps(left_pwm);
    const float right_rps = pwm_to_rps(right_pwm);
    const float v_left = (float)left_dir * left_rps * 2.0f * (float)M_PI * WHEEL_RADIUS_M;
    const float v_right = (float)right_dir * right_rps * 2.0f * (float)M_PI * WHEEL_RADIUS_M;

    const float v = 0.5f * (v_left + v_right);
    const float omega_ccw = (v_right - v_left) / WHEEL_BASE_M;
    /* Positive theta is clockwise to match the screen coordinate frame. */
    const float omega = -omega_ccw * ANGULAR_SCALE;

    theta = wrap_angle(theta + omega * dt_s);

    /* Heading 0 means +Y axis */
    const float dx = v * sinf(theta) * dt_s;
    const float dy = v * cosf(theta) * dt_s;

    portENTER_CRITICAL(&odom_lock);
    odom.x_m += dx;
    odom.y_m += dy;
    odom.theta_rad = theta;
    odom.v_mps = v;
    odom.omega_rps = omega;
    odom.v_left_mps = v_left;
    odom.v_right_mps = v_right;
    portEXIT_CRITICAL(&odom_lock);
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

static void init_pwm(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel_a = {
        .gpio_num = MOTOR_A_ENA,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_a);

    ledc_channel_config_t ledc_channel_b = {
        .gpio_num = MOTOR_B_ENB,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_b);
}

static void init_motors(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << MOTOR_A_PIN1) |
            (1ULL << MOTOR_A_PIN2) |
            (1ULL << MOTOR_B_PIN1) |
            (1ULL << MOTOR_B_PIN2),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    init_pwm();
}

static void set_motor_speeds(uint8_t motor_a_speed, uint8_t motor_b_speed) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, motor_a_speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, motor_b_speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

static uint8_t diagonal_inner_speed(void) {
    uint16_t scaled_speed = ((uint16_t)current_speed * DIAGONAL_INNER_SPEED_PERCENT) / 100;
    if (scaled_speed > 0 && scaled_speed < MIN_SPEED) {
        return MIN_SPEED;
    }
    return (uint8_t)scaled_speed;
}

static void apply_wheel_command(int8_t left_dir, uint8_t left_pwm, int8_t right_dir, uint8_t right_pwm) {
    int8_t a_dir = 0;
    int8_t b_dir = 0;
    uint8_t a_pwm = 0;
    uint8_t b_pwm = 0;

    if (MOTOR_A_IS_LEFT) {
        a_dir = left_dir * MOTOR_A_FORWARD_SIGN;
        b_dir = right_dir * MOTOR_B_FORWARD_SIGN;
        a_pwm = left_pwm;
        b_pwm = right_pwm;
    } else {
        a_dir = right_dir * MOTOR_A_FORWARD_SIGN;
        b_dir = left_dir * MOTOR_B_FORWARD_SIGN;
        a_pwm = right_pwm;
        b_pwm = left_pwm;
    }

    set_motor_dir_pins(MOTOR_A_PIN1, MOTOR_A_PIN2, a_dir);
    set_motor_dir_pins(MOTOR_B_PIN1, MOTOR_B_PIN2, b_dir);
    set_motor_speeds(a_pwm, b_pwm);
    odom_set_command(left_dir, left_pwm, right_dir, right_pwm);
}

static void move_robot(const char *cmd) {
    if (cmd == NULL) {
        return;
    }

    strncpy(last_motion, cmd, sizeof(last_motion) - 1);
    last_motion[sizeof(last_motion) - 1] = '\0';

    if (strcasecmp(cmd, "forward") == 0 || strcasecmp(cmd, "f") == 0) {
        apply_wheel_command(1, current_speed, 1, current_speed);
        ESP_LOGI(TAG, "MOVE FORWARD (speed: %d)", current_speed);
    } else if (strcasecmp(cmd, "back") == 0 || strcasecmp(cmd, "backward") == 0 || strcasecmp(cmd, "b") == 0) {
        apply_wheel_command(-1, current_speed, -1, current_speed);
        ESP_LOGI(TAG, "MOVE BACKWARD (speed: %d)", current_speed);
    } else if (strcasecmp(cmd, "forward_right") == 0 || strcasecmp(cmd, "fr") == 0) {
        apply_wheel_command(1, current_speed, 1, diagonal_inner_speed());
        ESP_LOGI(TAG, "MOVE FORWARD-RIGHT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "forward_left") == 0 || strcasecmp(cmd, "fl") == 0) {
        apply_wheel_command(1, diagonal_inner_speed(), 1, current_speed);
        ESP_LOGI(TAG, "MOVE FORWARD-LEFT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "back_right") == 0 || strcasecmp(cmd, "br") == 0) {
        apply_wheel_command(-1, current_speed, -1, diagonal_inner_speed());
        ESP_LOGI(TAG, "MOVE BACK-RIGHT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "back_left") == 0 || strcasecmp(cmd, "bl") == 0) {
        apply_wheel_command(-1, diagonal_inner_speed(), -1, current_speed);
        ESP_LOGI(TAG, "MOVE BACK-LEFT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "left") == 0 || strcasecmp(cmd, "l") == 0) {
        apply_wheel_command(-1, current_speed, 1, current_speed);
        ESP_LOGI(TAG, "MOVE LEFT (speed: %d)", current_speed);
    } else if (strcasecmp(cmd, "right") == 0 || strcasecmp(cmd, "r") == 0) {
        apply_wheel_command(1, current_speed, -1, current_speed);
        ESP_LOGI(TAG, "MOVE RIGHT (speed: %d)", current_speed);
    } else {
        apply_wheel_command(0, 0, 0, 0);
        ESP_LOGI(TAG, "STOP");
    }
}

static void set_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS, GET");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static void extract_json_string_value(const char *json_str, const char *key, char *out, size_t out_size) {
    if (!json_str || !key || !out || out_size == 0) {
        return;
    }

    out[0] = '\0';

    char pattern[40];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *key_pos = strstr(json_str, pattern);
    if (!key_pos) {
        return;
    }

    const char *colon = strchr(key_pos, ':');
    if (!colon) {
        return;
    }

    const char *value_start = strchr(colon, '"');
    if (!value_start) {
        return;
    }
    value_start++;

    const char *value_end = strchr(value_start, '"');
    if (!value_end) {
        return;
    }

    size_t value_len = (size_t)(value_end - value_start);
    if (value_len >= out_size) {
        value_len = out_size - 1;
    }

    memcpy(out, value_start, value_len);
    out[value_len] = '\0';
}

static void parse_ws_json(const char *json_str, char *action, char *direction, uint8_t *speed_val) {
    action[0] = '\0';
    direction[0] = '\0';
    *speed_val = 0;

    extract_json_string_value(json_str, "action", action, 32);
    extract_json_string_value(json_str, "direction", direction, 32);

    const char *speed_start = strstr(json_str, "\"speed\":");
    if (speed_start) {
        int speed_int = 0;
        if (sscanf(speed_start, "\"speed\":%d", &speed_int) == 1) {
            *speed_val = (uint8_t)speed_int;
        }
    }
}

static bool ws_is_connected(int *fd_out) {
    bool connected = false;
    int fd = -1;
    portENTER_CRITICAL(&ws_lock);
    connected = ws_client_connected;
    fd = ws_client_fd;
    portEXIT_CRITICAL(&ws_lock);
    if (fd_out) {
        *fd_out = fd;
    }
    return connected && fd >= 0;
}

static void ws_set_client(int fd, bool connected) {
    portENTER_CRITICAL(&ws_lock);
    ws_client_fd = connected ? fd : -1;
    ws_client_connected = connected;
    portEXIT_CRITICAL(&ws_lock);
}

static void ws_send_text(const char *text) {
    int fd = -1;
    if (!ws_is_connected(&fd) || http_server == NULL) {
        return;
    }

    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)text,
        .len = strlen(text),
    };

    esp_err_t err = httpd_ws_send_frame_async(http_server, fd, &frame);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WS send failed: %d", err);
        ws_set_client(-1, false);
    }
}

static void build_pose_json(char *out, size_t out_len, const odom_state_t *pose, int64_t now_ms) {
    const float theta_deg = pose->theta_rad * 180.0f / (float)M_PI;
    snprintf(out, out_len,
             "{\"type\":\"pose\",\"x\":%.3f,\"y\":%.3f,\"theta\":%.4f,\"theta_deg\":%.2f,"
             "\"v\":%.3f,\"omega\":%.3f,\"pwm_left\":%u,\"pwm_right\":%u,\"ts_ms\":%lld}",
             (double)pose->x_m,
             (double)pose->y_m,
             (double)pose->theta_rad,
             (double)theta_deg,
             (double)pose->v_mps,
             (double)pose->omega_rps,
             pose->pwm_left,
             pose->pwm_right,
             (long long)now_ms);
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        const int fd = httpd_req_to_sockfd(req);
        ws_set_client(fd, true);
        ESP_LOGI(TAG, "WS client connected (fd=%d)", fd);
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t buf[256] = {0};

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get length: %d", ret);
        return ret;
    }

    if (ws_pkt.len >= sizeof(buf)) {
        ESP_LOGW(TAG, "WS payload too large: %u", (unsigned int)ws_pkt.len);
        return ESP_ERR_INVALID_SIZE;
    }

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ws_set_client(-1, false);
        ESP_LOGI(TAG, "WS client closed");
        return ESP_OK;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        buf[ws_pkt.len] = '\0';
        ESP_LOGI(TAG, "WS text: %s", (char *)buf);

        char action[32];
        char direction[32];
        uint8_t speed_val = 0;
        parse_ws_json((const char *)buf, action, direction, &speed_val);

        if (strlen(action) == 0) {
            const char *response = "{\"error\":\"invalid json\"}";
            httpd_ws_frame_t rsp_pkt = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)response,
                .len = strlen(response),
            };
            httpd_ws_send_frame(req, &rsp_pkt);
            return ESP_OK;
        }

        if (strcmp(action, "move") == 0) {
            if (strlen(direction) > 0) {
                move_robot(direction);
            }
        } else if (strcmp(action, "stop") == 0) {
            move_robot("stop");
        } else if (strcmp(action, "speed_up") == 0) {
            if (current_speed < MAX_SPEED - SPEED_STEP) {
                current_speed += SPEED_STEP;
            } else {
                current_speed = MAX_SPEED;
            }
            ESP_LOGI(TAG, "Speed up to %d", current_speed);
            if (strcasecmp(last_motion, "stop") != 0) {
                move_robot(last_motion);
            }
        } else if (strcmp(action, "speed_down") == 0) {
            if (current_speed > MIN_SPEED + SPEED_STEP) {
                current_speed -= SPEED_STEP;
            } else {
                current_speed = MIN_SPEED;
            }
            ESP_LOGI(TAG, "Speed down to %d", current_speed);
            if (strcasecmp(last_motion, "stop") != 0) {
                move_robot(last_motion);
            }
        } else if (strcmp(action, "set_speed") == 0) {
            if (speed_val >= MIN_SPEED && speed_val <= MAX_SPEED) {
                current_speed = speed_val;
                ESP_LOGI(TAG, "Speed set to %d", current_speed);
                if (strcasecmp(last_motion, "stop") != 0) {
                    move_robot(last_motion);
                }
            } else {
                ESP_LOGW(TAG, "Speed out of range: %d", speed_val);
            }
        } else if (strcmp(action, "reset_pose") == 0) {
            odom_reset();
            ESP_LOGI(TAG, "Pose reset to 0,0,0");
        }

        odom_state_t snapshot = {0};
        odom_snapshot(&snapshot);
        char response[192];
        snprintf(response, sizeof(response),
                 "{\"status\":\"ok\",\"speed\":%d,\"action\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"theta\":%.4f}",
                 current_speed,
                 action,
                 (double)snapshot.x_m,
                 (double)snapshot.y_m,
                 (double)snapshot.theta_rad);

        httpd_ws_frame_t rsp_pkt = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)response,
            .len = strlen(response),
        };
        httpd_ws_send_frame(req, &rsp_pkt);
    }

    return ESP_OK;
}

static esp_err_t move_options_handler(httpd_req_t *req) {
    set_cors_headers(req);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t move_post_handler(httpd_req_t *req) {
    char body[128] = {0};
    int len = req->content_len;

    if (len <= 0 || len >= (int)sizeof(body)) {
        set_cors_headers(req);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"invalid payload\"}");
        return ESP_OK;
    }

    int received = 0;
    while (received < len) {
        int r = httpd_req_recv(req, body + received, len - received);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            set_cors_headers(req);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"recv failed\"}");
            return ESP_OK;
        }
        received += r;
    }
    body[received] = '\0';

    const char *direction = NULL;
    if (strstr(body, "forward_right")) direction = "forward_right";
    else if (strstr(body, "forward_left")) direction = "forward_left";
    else if (strstr(body, "back_right")) direction = "back_right";
    else if (strstr(body, "back_left")) direction = "back_left";
    else if (strstr(body, "forward")) direction = "forward";
    else if (strstr(body, "backward") || strstr(body, "back")) direction = "back";
    else if (strstr(body, "left")) direction = "left";
    else if (strstr(body, "right")) direction = "right";
    else if (strstr(body, "stop")) direction = "stop";

    if (!direction) {
        set_cors_headers(req);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"unknown direction\"}");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "HTTP RX: %s", direction);
    move_robot(direction);

    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "HTTP endpoint: http://" IPSTR ":8000/move", IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "WebSocket endpoint: ws://" IPSTR ":8000/ws", IP2STR(&event->ip_info.ip));
    }
}

static void wifi_init_sta(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(20000));

    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGW(TAG, "WiFi connect timeout. Check WIFI_SSID/WIFI_PASS in main.c");
    }
}

static void start_http_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8000;
    config.max_open_sockets = 4;

    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };
    httpd_register_uri_handler(http_server, &ws);

    httpd_uri_t move_post = {
        .uri = "/move",
        .method = HTTP_POST,
        .handler = move_post_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t move_options = {
        .uri = "/move",
        .method = HTTP_OPTIONS,
        .handler = move_options_handler,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &move_post));
    ESP_ERROR_CHECK(httpd_register_uri_handler(http_server, &move_options));
    ESP_LOGI(TAG, "HTTP server ready on port 8000 (POST /move, WS /ws)");
}

static void odom_task(void *arg) {
    int64_t last_us = esp_timer_get_time();
    int64_t last_tx_us = last_us;

    while (1) {
        const int64_t now_us = esp_timer_get_time();
        const float dt_s = (float)(now_us - last_us) / 1000000.0f;
        last_us = now_us;

        odom_update(dt_s);

        if ((now_us - last_tx_us) >= (int64_t)POSE_TX_MS * 1000) {
            last_tx_us = now_us;

            odom_state_t snapshot = {0};
            odom_snapshot(&snapshot);
            char msg[192];
            build_pose_json(msg, sizeof(msg), &snapshot, now_us / 1000);
            ws_send_text(msg);
        }

        vTaskDelay(pdMS_TO_TICKS(ODOM_UPDATE_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Lab 3 Ej4: WS pose streaming");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    init_motors();
    odom_reset();
    wifi_init_sta();
    start_http_server();

    xTaskCreate(odom_task, "odom_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready. Connect to ws://IP:8000/ws");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
