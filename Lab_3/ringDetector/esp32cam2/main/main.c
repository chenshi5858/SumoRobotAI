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

static const char *TAG = "ring_cam2";

static char s_last_logged_status[16] = "";
static int64_t s_last_log_ms = 0;

static esp_err_t init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static void vision_task(void *arg) {
    (void)arg;

    ESP_LOGI(TAG, "Black pixel edge detector started");
    ESP_LOGI(TAG, "  BLACK_THRESHOLD   = %d", BLACK_THRESHOLD);
    ESP_LOGI(TAG, "  BLACK_MIN_PERCENT = %d%%", BLACK_MIN_PERCENT);

    while (1) {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) {
            ESP_LOGW(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        edge_metrics_t metrics;
        const bool edge_detected = detect_edge(frame, &metrics);

        esp_camera_fb_return(frame);

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
                         "Vision status=%s black=%" PRIu32 "/%" PRIu32 " (%" PRIu32 "%%)",
                         status,
                         metrics.black_pixels,
                         metrics.total_pixels,
                         metrics.black_percent);
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
    ESP_LOGI(TAG, "ESP32-CAM black pixel edge detector starting");
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_camera());
    ESP_ERROR_CHECK(init_espnow());

    xTaskCreate(vision_task, "vision_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready. Sending 1 (borde) / 0 (plancha) via ESP-NOW");
}
