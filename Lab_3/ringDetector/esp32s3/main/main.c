#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app_config.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "espnow_receiver.h"
#include "motor_control.h"
#include "nvs_flash.h"
#include "ring_logic.h"
#include "robot_state.h"
#include "web_server.h"
#include "wifi_app.h"

static const char *TAG = "ring_s3";

static esp_err_t init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static void odom_task(void *arg) {
    (void)arg;

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
            web_server_send_pose(&snapshot, now_us / 1000);
        }

        vTaskDelay(pdMS_TO_TICKS(ODOM_UPDATE_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Ring detector ESP32-S3 receiver starting");

    ESP_ERROR_CHECK(init_nvs());

    robot_state_init();
    ESP_ERROR_CHECK(motor_control_init());
    odom_reset();

    ESP_ERROR_CHECK(wifi_app_init_sta());
    ESP_ERROR_CHECK(web_server_start());

    QueueHandle_t status_queue = xQueueCreate(8, sizeof(espnow_status_msg_t));
    if (status_queue == NULL) {
        ESP_LOGE(TAG, "Could not create ESP-NOW status queue");
        return;
    }

    ESP_ERROR_CHECK(espnow_receiver_init(status_queue));
    ESP_ERROR_CHECK(ring_logic_start(status_queue));

    xTaskCreate(odom_task, "odom_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Ready. WebSocket and ESP-NOW are active");
}
