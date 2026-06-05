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
#include "espnow_comm.h"

static const char *TAG = "ring_cam";

static char s_last_logged_status[16] = "";
static int64_t s_last_log_ms = 0;
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

    while (1) {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) {
            ESP_LOGW(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
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
        const char *status = edge_detected ? "1" : "0";
        const bool status_changed = strcmp(s_last_logged_status, status) != 0;
        const int64_t now = esp_timer_get_time() / 1000;
        const bool time_to_send = (now - s_last_log_ms) > 500;

        if (status_changed || time_to_send) {
            esp_err_t err = send_status(status);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "ESP-NOW send(%s) failed: %s", status, esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG,
                         "Vision status=%s raw=%s edge=%" PRIu32 "/%" PRIu32 " (%" PRIu32 "%%) score=%u",
                         status,
                         raw_edge_detected ? "1" : "0",
                         metrics.edge_pixels,
                         metrics.total_pixels,
                         metrics.edge_percent,
                         s_edge_score);
            }
            if (status_changed) {
                strlcpy(s_last_logged_status, status, sizeof(s_last_logged_status));
            }
            s_last_log_ms = now;
        }
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
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
    ESP_LOGI(TAG, "ESP32-CAM Sobel edge detector starting");
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_camera());
    ESP_ERROR_CHECK(init_espnow());

    xTaskCreate(vision_task, "vision_task", 6144, NULL, 5, NULL);
    xTaskCreate(debug_cmd_task, "debug_cmd", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "Ready. Send 'd' over serial to dump edge map.");
}
