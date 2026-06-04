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
static uint8_t s_border_score = 0;
static bool s_debounced_border = false;

static esp_err_t init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static bool debounce_border_detection(bool raw_detected) {
    const uint8_t debounce_target = BORDER_DEBOUNCE_COUNT > 0 ? BORDER_DEBOUNCE_COUNT : 1;

    if (raw_detected) {
        if (s_border_score < debounce_target) {
            s_border_score++;
        }
    } else if (s_border_score > 0) {
        s_border_score--;
    }

    if (!s_debounced_border && s_border_score >= debounce_target) {
        s_debounced_border = true;
    } else if (s_debounced_border && s_border_score == 0) {
        s_debounced_border = false;
    }

    return s_debounced_border;
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

        ring_detection_metrics_t metrics;
        const bool raw_border_detected = detect_ring_border(frame, &metrics);
        esp_camera_fb_return(frame);

        const bool border_detected = debounce_border_detection(raw_border_detected);
        const char *status = border_detected ? "WHITE" : "SAFE";
        esp_err_t err = send_status(status);

        const int64_t now = esp_timer_get_time() / 1000;
        const bool status_changed = strcmp(s_last_logged_status, status) != 0;
        const bool periodic = (now - s_last_log_ms) > 1000;
        if (err != ESP_OK && (status_changed || periodic)) {
            ESP_LOGW(TAG, "ESP-NOW status send failed: %s", esp_err_to_name(err));
        } else if (err == ESP_OK && (status_changed || periodic)) {
            const uint32_t edge_percent =
                metrics.total_pixels > 0 ? (metrics.edge_pixels * 100U) / metrics.total_pixels : 0;
            ESP_LOGI(TAG,
                     "Vision status=%s raw=%s edge=%" PRIu32 "/%" PRIu32 " (%" PRIu32 "%%) "
                     "rows=%" PRIu32 " best=%" PRIu32 " edge_thr=%u trig=%s score=%u",
                     status,
                     raw_border_detected ? "EDGE" : "SAFE",
                     metrics.edge_pixels,
                     metrics.total_pixels,
                     edge_percent,
                     metrics.edge_rows,
                     metrics.best_row_pixels,
                     metrics.edge_threshold,
                     metrics.edge_trigger ? "+sobel" : "",
                     s_border_score);
        }
        if (status_changed || periodic) {
            strlcpy(s_last_logged_status, status, sizeof(s_last_logged_status));
            s_last_log_ms = now;
        }
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-CAM adaptive border detector starting");
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_camera());
    ESP_ERROR_CHECK(init_espnow());

    xTaskCreate(vision_task, "vision_task", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready: capturing 96x96 RGB565, detecting Sobel border, and sending ESP-NOW");
}
