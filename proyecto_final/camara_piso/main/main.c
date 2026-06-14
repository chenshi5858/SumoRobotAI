#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "camera_app.h"
#include "espnow_protocol.h"
#include "uart_comm.h"

static const char *TAG = "floor_cam";

static uint8_t s_edge_score = 0;
static bool s_debounced_edge = false;
static volatile bool s_debug_dump_requested = false;

static esp_err_t init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static bool debounce_edge_detection(bool raw_detected) {
    const uint8_t debounce_target = DEBOUNCE_COUNT > 0 ? DEBOUNCE_COUNT : 1;

    if (raw_detected) {
        if (s_edge_score < debounce_target) {
            s_edge_score++;
        }
    } else if (s_edge_score > 0) {
        s_edge_score--;
    }

    if (!s_debounced_edge && s_edge_score >= debounce_target) {
        s_debounced_edge = true;
    } else if (s_debounced_edge && s_edge_score == 0) {
        s_debounced_edge = false;
    }

    return s_debounced_edge;
}

static void vision_task(void *arg) {
    (void)arg;
    TickType_t last_wake_time = xTaskGetTickCount();
    uint8_t last_sent = 2;

    while (1) {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) {
            ESP_LOGW(TAG, "Camera capture failed");
            vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
            continue;
        }

        edge_metrics_t metrics;
        const bool raw_edge_detected = detect_edge(frame, &metrics);

        if (s_debug_dump_requested) {
            s_debug_dump_requested = false;
            debug_print_edge_map();
        }

        esp_camera_fb_return(frame);

        const bool edge_detected = debounce_edge_detection(raw_edge_detected);
        const uint8_t value = edge_detected ? FLOOR_EDGE : FLOOR_SAFE;

        if (value != last_sent) {
            esp_err_t err = uart_send_floor_status(value);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Floor status=%u edge%%=%" PRIu32 " rows=%" PRIu32,
                         value, metrics.edge_percent, metrics.active_rows);
                last_sent = value;
            } else {
                ESP_LOGW(TAG, "UART send failed: %s", esp_err_to_name(err));
            }
        } else {
            uart_send_floor_status(value);
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
    }
}

static void debug_cmd_task(void *arg) {
    (void)arg;
    int ch;
    while (1) {
        ch = getchar();
        if (ch == 'd' || ch == 'D') {
            s_debug_dump_requested = true;
            ESP_LOGI(TAG, "Debug dump requested");
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Floor detection camera starting");
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_camera());
    ESP_ERROR_CHECK(uart_init());

    xTaskCreate(vision_task, "vision_task", 6144, NULL, 5, NULL);
    xTaskCreate(debug_cmd_task, "debug_cmd", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "Ready. Sending floor status via UART.");
}
