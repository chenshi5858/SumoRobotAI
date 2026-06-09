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
    int64_t turn_start_at_ms = -1;
    int64_t safe_resume_at_ms = -1;
    char last_applied_status[16] = "";

    robot_state_set_ring_status("NO_LINK", NULL);
    stop_motors();

    while (1) {
        if (xQueueReceive(queue, &msg, pdMS_TO_TICKS(50)) == pdTRUE) {
            latest = msg;

            while (xQueueReceive(queue, &msg, 0) == pdTRUE) {
                latest = msg;
            }

            last_rx_ms = latest.rx_time_ms;
            robot_state_set_ring_status(latest.status, latest.source_mac);
            const int64_t now_ms = esp_timer_get_time() / 1000;

            if (strcmp(latest.status, "1") == 0) {
                safe_resume_at_ms = -1;
                if (strcmp(last_applied_status, "1") != 0 &&
                    strcmp(last_applied_status, "WAIT_TURN") != 0) {
                    ESP_LOGI(TAG, "BLACK detected: pausing %d ms before turning", RING_PRE_TURN_PAUSE_MS);
                    stop_motors();
                    turn_start_at_ms = now_ms + RING_PRE_TURN_PAUSE_MS;
                    strlcpy(last_applied_status, "WAIT_TURN", sizeof(last_applied_status));
                }
            } else if (strcmp(latest.status, "0") == 0) {
                if (strcmp(last_applied_status, "1") == 0) {
                    ESP_LOGI(TAG, "SAFE after edge: pausing %d ms before moving", RING_RECOVERY_PAUSE_MS);
                    stop_motors();
                    safe_resume_at_ms = now_ms + RING_RECOVERY_PAUSE_MS;
                    strlcpy(last_applied_status, "WAIT_SAFE", sizeof(last_applied_status));
                } else if (strcmp(last_applied_status, "WAIT_TURN") == 0) {
                    ESP_LOGI(TAG, "SAFE before turn: moving forward slowly");
                    turn_start_at_ms = -1;
                    motor_set_speed(RING_FOLLOW_SPEED);
                    move_forward();
                    strlcpy(last_applied_status, "0", sizeof(last_applied_status));
                } else if (safe_resume_at_ms < 0 && strcmp(last_applied_status, "0") != 0) {
                    ESP_LOGI(TAG, "SAFE detected: moving forward slowly");
                    motor_set_speed(RING_FOLLOW_SPEED);
                    move_forward();
                    strlcpy(last_applied_status, "0", sizeof(last_applied_status));
                }
            }
        }

        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (turn_start_at_ms > 0 && now_ms >= turn_start_at_ms) {
            ESP_LOGI(TAG, "Pre-turn pause finished: rotating left");
            rotate_fast();
            strlcpy(last_applied_status, "1", sizeof(last_applied_status));
            turn_start_at_ms = -1;
        }

        if (safe_resume_at_ms > 0 && now_ms >= safe_resume_at_ms) {
            ESP_LOGI(TAG, "Recovery pause finished: moving forward slowly");
            motor_set_speed(RING_FOLLOW_SPEED);
            move_forward();
            strlcpy(last_applied_status, "0", sizeof(last_applied_status));
            safe_resume_at_ms = -1;
        }

        if (last_rx_ms > 0 && (now_ms - last_rx_ms) > ESPNOW_STATUS_TIMEOUT_MS) {
            ESP_LOGW(TAG, "ESP-NOW camera timeout, stopping motors");
            stop_motors();
            robot_state_set_ring_status("NO_LINK", NULL);
            last_rx_ms = -1;
            turn_start_at_ms = -1;
            safe_resume_at_ms = -1;
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
