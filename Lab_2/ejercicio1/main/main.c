#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "EJ1_WIFI_MOTORES";

/* WiFi credentials: replace with your network values */

#define WIFI_SSID "Chenshi"
#define WIFI_PASS "Hola12345"

/*#define WIFI_SSID "LIB-0845386"
#define WIFI_PASS "fknJftkhUua9"*/

/*#define WIFI_SSID "wifi-campus"
#define WIFI_PASS "uandes2200"*/


#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;
static httpd_handle_t http_server = NULL;

/* Motor pins (adjust to your real wiring) */
#define MOTOR_A_PIN1 6
#define MOTOR_A_PIN2 7
#define MOTOR_B_PIN1 16
#define MOTOR_B_PIN2 17
#define MOTOR_A_ENA  9
#define MOTOR_B_ENB  10

/* PWM Speed values: 0-255 */
static uint8_t current_speed = 255;
static const uint8_t MAX_SPEED = 255;
static const uint8_t MIN_SPEED = 50;
static const uint8_t SPEED_STEP = 15;
static const uint8_t DIAGONAL_INNER_SPEED_PERCENT = 45;

static void init_pwm(void) {
    /* LEDC Timer configuration */
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    /* LEDC Channel for Motor A (ENA on pin 9) */
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

    /* LEDC Channel for Motor B (ENB on pin 10) */
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
    
    /* Initialize PWM */
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

static void set_motor_speed(void) {
    set_motor_speeds(current_speed, current_speed);
    ESP_LOGI(TAG, "Speed set to %d/255", current_speed);
}

static void move_robot(const char *cmd) {
    if (cmd == NULL) return;

    if (strcasecmp(cmd, "forward") == 0 || strcasecmp(cmd, "f") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2,  1);
        set_motor_speed();
        ESP_LOGI(TAG, "MOVE FORWARD (speed: %d)", current_speed);
    } else if (strcasecmp(cmd, "back") == 0 || strcasecmp(cmd, "backward") == 0 || strcasecmp(cmd, "b") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
        set_motor_speed();
        ESP_LOGI(TAG, "MOVE BACKWARD (speed: %d)", current_speed);
    } else if (strcasecmp(cmd, "forward_right") == 0 || strcasecmp(cmd, "fr") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 1);
        set_motor_speeds(diagonal_inner_speed(), current_speed);
        ESP_LOGI(TAG, "MOVE FORWARD-RIGHT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "forward_left") == 0 || strcasecmp(cmd, "fl") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 1);
        set_motor_speeds(current_speed, diagonal_inner_speed());
        ESP_LOGI(TAG, "MOVE FORWARD-LEFT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "back_right") == 0 || strcasecmp(cmd, "br") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
        set_motor_speeds(diagonal_inner_speed(), current_speed);
        ESP_LOGI(TAG, "MOVE BACK-RIGHT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "back_left") == 0 || strcasecmp(cmd, "bl") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
        set_motor_speeds(current_speed, diagonal_inner_speed());
        ESP_LOGI(TAG, "MOVE BACK-LEFT (speed: %d, inner: %d)", current_speed, diagonal_inner_speed());
    } else if (strcasecmp(cmd, "left") == 0 || strcasecmp(cmd, "l") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
        set_motor_speed();
        ESP_LOGI(TAG, "MOVE LEFT (speed: %d)", current_speed);
    } else if (strcasecmp(cmd, "right") == 0 || strcasecmp(cmd, "r") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 1);
        set_motor_speed();
        ESP_LOGI(TAG, "MOVE RIGHT (speed: %d)", current_speed);
    } else {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 0);
        set_motor_speeds(0, 0);
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

/* Simple JSON parser for WebSocket commands */
static void parse_ws_json(const char *json_str, char *action, char *direction, uint8_t *speed_val) {
    action[0] = '\0';
    direction[0] = '\0';
    *speed_val = 0;

    extract_json_string_value(json_str, "action", action, 32);
    extract_json_string_value(json_str, "direction", direction, 32);

    /* Extract speed */
    const char *speed_start = strstr(json_str, "\"speed\":");
    if (speed_start) {
        int speed_int = 0;
        if (sscanf(speed_start, "\"speed\":%d", &speed_int) == 1) {
            *speed_val = (uint8_t)speed_int;
        }
    }
}

/* WebSocket handler for /ws */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WS Client connected");
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

    ESP_LOGI(TAG, "WS received: opcode=%d, len=%d", ws_pkt.type, ws_pkt.len);

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        buf[ws_pkt.len] = '\0';
        ESP_LOGI(TAG, "WS text: %s", (char *)buf);

        char action[32], direction[32];
        uint8_t speed_val = 0;
        parse_ws_json((const char *)buf, action, direction, &speed_val);

        if (strlen(action) == 0) {
            ESP_LOGE(TAG, "No action in JSON");
            const char *response = "{\"error\":\"invalid json\"}";
            httpd_ws_frame_t rsp_pkt = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)response,
                .len = strlen(response),
            };
            httpd_ws_send_frame(req, &rsp_pkt);
            return ESP_OK;
        }

        /* Handle move action */
        if (strcmp(action, "move") == 0) {
            if (strlen(direction) > 0) {
                move_robot(direction);
            }
        }
        /* Handle stop action */
        else if (strcmp(action, "stop") == 0) {
            move_robot("stop");
        }
        /* Handle speed increase */
        else if (strcmp(action, "speed_up") == 0) {
            if (current_speed < MAX_SPEED - SPEED_STEP) {
                current_speed += SPEED_STEP;
            } else {
                current_speed = MAX_SPEED;
            }
            ESP_LOGI(TAG, "Speed up to %d", current_speed);
            set_motor_speed();
        }
        /* Handle speed decrease */
        else if (strcmp(action, "speed_down") == 0) {
            if (current_speed > MIN_SPEED + SPEED_STEP) {
                current_speed -= SPEED_STEP;
            } else {
                current_speed = MIN_SPEED;
            }
            ESP_LOGI(TAG, "Speed down to %d", current_speed);
            set_motor_speed();
        }
        /* Handle set speed directly */
        else if (strcmp(action, "set_speed") == 0) {
            if (speed_val >= MIN_SPEED && speed_val <= MAX_SPEED) {
                current_speed = speed_val;
                ESP_LOGI(TAG, "Speed set to %d", current_speed);
                set_motor_speed();
            } else {
                ESP_LOGW(TAG, "Speed out of range: %d", speed_val);
            }
        }

        /* Send status response */
        char response[128];
        snprintf(response, sizeof(response),
                 "{\"status\":\"ok\",\"speed\":%d,\"action\":\"%s\"}", 
                 current_speed, action);
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
    /* Max 7 allowed in this IDF config (3 are used internally by HTTP server) */
    config.max_open_sockets = 4;

    ESP_ERROR_CHECK(httpd_start(&http_server, &config));

    /* Register WebSocket endpoint */
    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };
    httpd_register_uri_handler(http_server, &ws);

    /* Register POST endpoint for backward compatibility */
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

void app_main(void) {
    ESP_LOGI(TAG, "Inicio firmware WiFi - control motores");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    init_motors();
    wifi_init_sta();
    start_http_server();

    ESP_LOGI(TAG, "Listo para control por WebSocket. Conecta a ws://IP:8000/ws");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
