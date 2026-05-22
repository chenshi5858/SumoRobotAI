#include "ring_logic.h"

#include <string.h>

#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "espnow_receiver.h"
#include "motor_control.h"
#include "robot_state.h"

static const char *TAG = "ring_logic";

static void ring_control_task(void *arg) {
    QueueHandle_t queue = (QueueHandle_t)arg;
    espnow_status_msg_t msg = {0};
    espnow_status_msg_t latest = {0};
    int64_t last_rx_ms = -1;
    char last_applied_status[16] = "";

    robot_state_set_ring_status("NO_LINK", NULL);
    stop_motors();

    while (1) {
        if (xQueueReceive(queue, &msg, pdMS_TO_TICKS(200)) == pdTRUE) {
            latest = msg;

            /*
             * Si la camara envia rapido, usamos el dato mas reciente y evitamos
             * actuar sobre mensajes viejos acumulados en la cola.
             */
            while (xQueueReceive(queue, &msg, 0) == pdTRUE) {
                latest = msg;
            }

            last_rx_ms = latest.rx_time_ms;
            robot_state_set_ring_status(latest.status, latest.source_mac);

            if (strcmp(latest.status, "WHITE") == 0) {
                ESP_LOGI(TAG, "WHITE detected: rotating to stay inside the ring");
                rotate_fast();
                vTaskDelay(pdMS_TO_TICKS(AVOID_ROTATE_MS));
                stop_motors();
                strlcpy(last_applied_status, "WHITE", sizeof(last_applied_status));
            } else if (strcmp(latest.status, "SAFE") == 0) {
                if (strcmp(last_applied_status, "SAFE") != 0) {
                    ESP_LOGI(TAG, "SAFE detected: moving forward");
                    move_forward();
                    strlcpy(last_applied_status, "SAFE", sizeof(last_applied_status));
                }
            }
        }

        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (last_rx_ms > 0 && (now_ms - last_rx_ms) > ESPNOW_STATUS_TIMEOUT_MS) {
            ESP_LOGW(TAG, "ESP-NOW camera timeout, stopping motors");
            stop_motors();
            robot_state_set_ring_status("NO_LINK", NULL);
            last_rx_ms = -1;
            last_applied_status[0] = '\0';
        }
    }
}

esp_err_t ring_logic_start(QueueHandle_t status_queue) {
    if (status_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    BaseType_t ok = xTaskCreate(ring_control_task, "ring_logic", 4096, status_queue, 6, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Ring control task started");
    return ESP_OK;
}
