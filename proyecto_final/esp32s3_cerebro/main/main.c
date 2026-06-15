#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "coordinator.h"
#include "espnow_protocol.h"
#include "main_config.h"
#include "microphone_handler.h"
#include "motor_control.h"
#include "robot_state.h"
#include "uart_receiver.h"

static const char *TAG = "cerebro";

static esp_err_t init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Sumo Brain starting");

    ESP_ERROR_CHECK(init_nvs());
    robot_state_init();
    ESP_ERROR_CHECK(motor_control_init());
    odom_reset();

    QueueHandle_t cam_queue = xQueueCreate(16, sizeof(camera_msg_t));
    if (cam_queue == NULL) {
        ESP_LOGE(TAG, "Could not create camera queue");
        return;
    }

    QueueHandle_t mic_cmd_queue = xQueueCreate(8, sizeof(mic_command_event_t));
    if (mic_cmd_queue == NULL) {
        ESP_LOGE(TAG, "Could not create mic command queue");
        return;
    }

    uart_cam_config_t floor_cam_config = {
        .uart_num = UART_NUM_1,
        .rx_gpio = UART1_RX_GPIO,
        .camera_id = CAMERA_ID_FLOOR,
    };

    uart_cam_config_t beacon_cam_config = {
        .uart_num = UART_NUM_2,
        .rx_gpio = UART2_RX_GPIO,
        .camera_id = CAMERA_ID_BEACON,
    };

    ESP_ERROR_CHECK(uart_receiver_start(&floor_cam_config, cam_queue));
    ESP_ERROR_CHECK(uart_receiver_start(&beacon_cam_config, cam_queue));
    ESP_ERROR_CHECK(coordinator_start(cam_queue, mic_cmd_queue));

    esp_err_t mic_err = microphone_init(mic_cmd_queue);
    if (mic_err == ESP_OK) {
        microphone_start_task();
    } else {
        ESP_LOGW(TAG, "Microphone init failed (%s), continuing without audio", esp_err_to_name(mic_err));
    }

    ESP_LOGI(TAG, "System ready. Waiting for camera links via UART...");
}
