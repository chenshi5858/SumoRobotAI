#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "espnow_receiver.h"
#include "motor_control.h"
#include "nvs_flash.h"
#include "ring_logic.h"
#include "robot_state.h"
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

void app_main(void) {
    ESP_LOGI(TAG, "Autonomous ring detector ESP32-S3 starting");

    ESP_ERROR_CHECK(init_nvs());

    robot_state_init();

    ESP_ERROR_CHECK(wifi_app_init_sta());
    ESP_ERROR_CHECK(motor_control_init());
    odom_reset();

    QueueHandle_t status_queue = xQueueCreate(8, sizeof(espnow_status_msg_t));
    if (status_queue == NULL) {
        ESP_LOGE(TAG, "Could not create status queue");
        return;
    }

    ESP_ERROR_CHECK(espnow_receiver_init(status_queue));
    ESP_ERROR_CHECK(ring_logic_start(status_queue));

    ESP_LOGI(TAG, "Ready. Autonomous ESP-NOW control is active");
}
