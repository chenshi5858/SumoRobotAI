#include "web_server.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "robot_state.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "web_server";

static httpd_handle_t s_http_server = NULL;
static int s_ws_client_fd = -1;
static bool s_ws_client_connected = false;
static portMUX_TYPE s_ws_lock = portMUX_INITIALIZER_UNLOCKED;

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
            if (speed_int < 0) {
                speed_int = 0;
            } else if (speed_int > 255) {
                speed_int = 255;
            }
            *speed_val = (uint8_t)speed_int;
        }
    }
}

static bool ws_is_connected(int *fd_out) {
    bool connected = false;
    int fd = -1;

    portENTER_CRITICAL(&s_ws_lock);
    connected = s_ws_client_connected;
    fd = s_ws_client_fd;
    portEXIT_CRITICAL(&s_ws_lock);

    if (fd_out) {
        *fd_out = fd;
    }
    return connected && fd >= 0;
}

static void ws_set_client(int fd, bool connected) {
    portENTER_CRITICAL(&s_ws_lock);
    s_ws_client_fd = connected ? fd : -1;
    s_ws_client_connected = connected;
    portEXIT_CRITICAL(&s_ws_lock);
}

static void ws_send_text(const char *text) {
    int fd = -1;
    if (!ws_is_connected(&fd) || s_http_server == NULL) {
        return;
    }

    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)text,
        .len = strlen(text),
    };

    esp_err_t err = httpd_ws_send_frame_async(s_http_server, fd, &frame);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WS send failed: %s", esp_err_to_name(err));
        ws_set_client(-1, false);
    }
}

static void build_pose_json(char *out, size_t out_len, const odom_state_t *pose, int64_t now_ms) {
    robot_status_snapshot_t status = {0};
    robot_state_get_snapshot(&status);

    const float theta_deg = pose->theta_rad * 180.0f / (float)M_PI;
    const int64_t espnow_age_ms =
        status.last_update_ms >= 0 ? now_ms - status.last_update_ms : -1;

    char camera_mac[18] = "unknown";
    if (status.have_source_mac) {
        snprintf(camera_mac, sizeof(camera_mac),
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 status.source_mac[0],
                 status.source_mac[1],
                 status.source_mac[2],
                 status.source_mac[3],
                 status.source_mac[4],
                 status.source_mac[5]);
    }

    snprintf(out, out_len,
             "{\"type\":\"pose\",\"x\":%.3f,\"y\":%.3f,\"theta\":%.4f,\"theta_deg\":%.2f,"
             "\"v\":%.3f,\"omega\":%.3f,\"pwm_left\":%u,\"pwm_right\":%u,"
             "\"ring_status\":\"%s\",\"motion\":\"%s\",\"camera_mac\":\"%s\","
             "\"espnow_age_ms\":%lld,\"speed\":%u,\"ts_ms\":%lld}",
             (double)pose->x_m,
             (double)pose->y_m,
             (double)pose->theta_rad,
             (double)theta_deg,
             (double)pose->v_mps,
             (double)pose->omega_rps,
             pose->pwm_left,
             pose->pwm_right,
             status.ring_status,
             status.motion,
             camera_mac,
             (long long)espnow_age_ms,
             motor_get_speed(),
             (long long)now_ms);
}

void web_server_send_pose(const odom_state_t *pose, int64_t now_ms) {
    if (pose == NULL) {
        return;
    }

    char msg[384];
    build_pose_json(msg, sizeof(msg), pose, now_ms);
    ws_send_text(msg);
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
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get length: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len >= sizeof(buf)) {
        ESP_LOGW(TAG, "WS payload too large: %u", (unsigned int)ws_pkt.len);
        return ESP_ERR_INVALID_SIZE;
    }

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
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
            stop_motors();
        } else if (strcmp(action, "speed_up") == 0) {
            motor_speed_up();
        } else if (strcmp(action, "speed_down") == 0) {
            motor_speed_down();
        } else if (strcmp(action, "set_speed") == 0) {
            motor_set_speed(speed_val);
        } else if (strcmp(action, "reset_pose") == 0) {
            odom_reset();
            ESP_LOGI(TAG, "Pose reset to 0,0,0");
        }

        odom_state_t snapshot = {0};
        odom_snapshot(&snapshot);
        char response[224];
        snprintf(response, sizeof(response),
                 "{\"status\":\"ok\",\"speed\":%u,\"action\":\"%s\",\"x\":%.3f,\"y\":%.3f,\"theta\":%.4f}",
                 motor_get_speed(),
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

esp_err_t web_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.max_open_sockets = 4;

    esp_err_t err = httpd_start(&s_http_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &ws));

    httpd_uri_t move_post = {
        .uri = "/move",
        .method = HTTP_POST,
        .handler = move_post_handler,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &move_post));

    httpd_uri_t move_options = {
        .uri = "/move",
        .method = HTTP_OPTIONS,
        .handler = move_options_handler,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_http_server, &move_options));

    ESP_LOGI(TAG, "HTTP server ready on port %d (POST /move, WS /ws)", WEB_SERVER_PORT);
    return ESP_OK;
}
