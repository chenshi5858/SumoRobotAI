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
static uint8_t s_black_score = 0;
static bool s_debounced_black = false;

static esp_err_t init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static bool debounce_black_detection(bool raw_detected) {
    const uint8_t debounce_target = DEBOUNCE_COUNT > 0 ? DEBOUNCE_COUNT : 1;

    if (raw_detected) {
        if (s_black_score < debounce_target) {
            s_black_score++;
        }
    } else if (s_black_score > 0) {
        s_black_score--;
    }

    if (!s_debounced_black && s_black_score >= debounce_target) {
        s_debounced_black = true;
    } else if (s_debounced_black && s_black_score == 0) {
        s_debounced_black = false;
    }

    return s_debounced_black;
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

        black_line_metrics_t metrics;
        const bool raw_black_detected = detect_black_line(frame, &metrics);
        esp_camera_fb_return(frame);

        const bool black_detected = debounce_black_detection(raw_black_detected);
        const char *status = black_detected ? "1" : "0";
        const bool status_changed = strcmp(s_last_logged_status, status) != 0;
        const int64_t now = esp_timer_get_time() / 1000;
        const bool time_to_send = (now - s_last_log_ms) > 500;

        if (status_changed || time_to_send) {
            esp_err_t err = send_status(status);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "ESP-NOW send(%s) failed: %s", status, esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG,
                         "Vision status=%s raw=%s black=%" PRIu32 "/%" PRIu32 " (%" PRIu32 "%%) score=%u",
                         status,
                         raw_black_detected ? "1" : "0",
                         metrics.black_pixels,
                         metrics.total_pixels,
                         metrics.black_percent,
                         s_black_score);
            }
            if (status_changed) {
                strlcpy(s_last_logged_status, status, sizeof(s_last_logged_status));
            }
            s_last_log_ms = now;
        }
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-CAM black line detector starting");
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_camera());
    ESP_ERROR_CHECK(init_espnow());

    xTaskCreate(vision_task, "vision_task", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready: capturing 96x96 RGB565, detecting black line, and sending via ESP-NOW");
}
