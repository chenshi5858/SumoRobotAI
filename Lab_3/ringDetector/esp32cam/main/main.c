#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "app_config.h"
#include "camera_app.h"
#include "espnow_comm.h"

static const char *TAG = "ring_cam";

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

    while (1) {
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame == NULL) {
            ESP_LOGW(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        uint32_t white_pixels = 0;
        uint32_t total_pixels = 0;
        const bool white_detected = detect_white_region(frame, &white_pixels, &total_pixels);
        esp_camera_fb_return(frame);

        const char *status = white_detected ? "WHITE" : "SAFE";
        esp_err_t err = send_status(status);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ESP-NOW status send failed: %s", esp_err_to_name(err));
        }

        const uint32_t white_percent = total_pixels > 0 ? (white_pixels * 100U) / total_pixels : 0;
        ESP_LOGI(TAG,
                 "Vision status=%s white=%" PRIu32 "/%" PRIu32 " (%" PRIu32 "%%)",
                 status,
                 white_pixels,
                 total_pixels,
                 white_percent);
        vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Simple ESP32-CAM white detector starting");
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_camera());
    ESP_ERROR_CHECK(init_espnow());

    xTaskCreate(vision_task, "vision_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready: capturing 96x96 RGB565, logging white ROI, and sending ESP-NOW");
}
