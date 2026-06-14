#include "coordinator.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "main_config.h"
#include "mic_freq_cmds.h"
#include "motor_control.h"
#include "ring_logic.h"
#include "robot_state.h"
#include "uart_receiver.h"

static const char *TAG = "coordinator";

typedef struct {
    ring_context_t ring_ctx;
    int64_t beacon_last_seen_ms;
    uint8_t beacon_class_id;
    int64_t mic_cmd_expires_ms;
    QueueHandle_t mic_cmd_queue;
} coordinator_t;

static coordinator_t s_coord = {0};

static void handle_mic_command(mic_command_t cmd, int64_t now_ms) {
    switch (cmd) {
        case MIC_CMD_FULL_FORWARD:
            ESP_LOGI(TAG, "MIC CMD: FULL FORWARD");
            motor_set_speed(MAX_SPEED);
            move_forward_full();
            s_coord.mic_cmd_expires_ms = now_ms + 2000;
            break;
        case MIC_CMD_STOP:
            ESP_LOGI(TAG, "MIC CMD: STOP");
            stop_motors();
            s_coord.mic_cmd_expires_ms = now_ms + 5000;
            break;
        case MIC_CMD_ROTATE_LEFT:
            ESP_LOGI(TAG, "MIC CMD: ROTATE LEFT");
            rotate_left();
            s_coord.mic_cmd_expires_ms = now_ms + 1000;
            break;
        case MIC_CMD_ROTATE_RIGHT:
            ESP_LOGI(TAG, "MIC CMD: ROTATE RIGHT");
            rotate_right();
            s_coord.mic_cmd_expires_ms = now_ms + 1000;
            break;
        case MIC_CMD_BOOST:
            ESP_LOGI(TAG, "MIC CMD: BOOST SPEED");
            motor_set_speed(MAX_SPEED);
            s_coord.mic_cmd_expires_ms = now_ms + 3000;
            break;
        default:
            break;
    }
}

static void process_mic_commands(int64_t now_ms) {
    if (s_coord.mic_cmd_queue == NULL) {
        return;
    }

    mic_command_t cmd;
    while (xQueueReceive(s_coord.mic_cmd_queue, &cmd, 0) == pdTRUE) {
        handle_mic_command(cmd, now_ms);
    }
}

static void handle_floor_message(const camera_msg_t *cam_msg, int64_t now_ms) {
    const char *status = (cam_msg->msg.value == FLOOR_EDGE) ? "1" : "0";
    ring_context_update(&s_coord.ring_ctx, status, cam_msg->rx_time_ms);

    if (strcmp(status, "1") == 0) {
        if (s_coord.ring_ctx.state == RING_STATE_SAFE ||
            s_coord.ring_ctx.state == RING_STATE_NO_LINK) {
            ESP_LOGI(TAG, "EDGE detected: pausing %d ms before turn", RING_PRE_TURN_PAUSE_MS);
            stop_motors();
            s_coord.ring_ctx.state = RING_STATE_EDGE_WAIT_TURN;
            s_coord.ring_ctx.wait_until_ms = now_ms + RING_PRE_TURN_PAUSE_MS;
            strlcpy(s_coord.ring_ctx.last_value, "1", sizeof(s_coord.ring_ctx.last_value));
        }
    } else if (strcmp(status, "0") == 0) {
        if (s_coord.ring_ctx.state == RING_STATE_EDGE_TURNING) {
            ESP_LOGI(TAG, "SAFE after edge: recovery pause %d ms", RING_RECOVERY_PAUSE_MS);
            stop_motors();
            s_coord.ring_ctx.state = RING_STATE_EDGE_RECOVERY;
            s_coord.ring_ctx.wait_until_ms = now_ms + RING_RECOVERY_PAUSE_MS;
            strlcpy(s_coord.ring_ctx.last_value, "0", sizeof(s_coord.ring_ctx.last_value));
        } else if (s_coord.ring_ctx.state == RING_STATE_EDGE_WAIT_TURN) {
            ESP_LOGI(TAG, "SAFE before turn: resuming forward");
            s_coord.ring_ctx.state = RING_STATE_SAFE;
            s_coord.ring_ctx.wait_until_ms = -1;
            motor_set_speed(RING_FOLLOW_SPEED);
            move_forward();
            strlcpy(s_coord.ring_ctx.last_value, "0", sizeof(s_coord.ring_ctx.last_value));
        } else if (s_coord.ring_ctx.state == RING_STATE_NO_LINK) {
            ESP_LOGI(TAG, "SAFE (link established): moving forward");
            s_coord.ring_ctx.state = RING_STATE_SAFE;
            motor_set_speed(RING_FOLLOW_SPEED);
            move_forward();
            strlcpy(s_coord.ring_ctx.last_value, "0", sizeof(s_coord.ring_ctx.last_value));
        }
    }
}

static void handle_beacon_message(const camera_msg_t *cam_msg) {
    s_coord.beacon_class_id = cam_msg->msg.value;
    s_coord.beacon_last_seen_ms = cam_msg->rx_time_ms;
    robot_state_set_beacon(cam_msg->msg.value);
}

static void process_ring_timers(int64_t now_ms) {
    if (s_coord.ring_ctx.state == RING_STATE_EDGE_WAIT_TURN &&
        s_coord.ring_ctx.wait_until_ms > 0 &&
        now_ms >= s_coord.ring_ctx.wait_until_ms) {
        ESP_LOGI(TAG, "Pre-turn done: rotating left");
        rotate_fast();
        s_coord.ring_ctx.state = RING_STATE_EDGE_TURNING;
        s_coord.ring_ctx.wait_until_ms = -1;
    }

    if (s_coord.ring_ctx.state == RING_STATE_EDGE_RECOVERY &&
        s_coord.ring_ctx.wait_until_ms > 0 &&
        now_ms >= s_coord.ring_ctx.wait_until_ms) {
        ESP_LOGI(TAG, "Recovery done: moving forward");
        s_coord.ring_ctx.state = RING_STATE_SAFE;
        s_coord.ring_ctx.wait_until_ms = -1;
        motor_set_speed(RING_FOLLOW_SPEED);
        move_forward();
    }

    if (ring_context_is_timed_out(&s_coord.ring_ctx, now_ms, ESPNOW_STATUS_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "Camera timeout, stopping motors");
        stop_motors();
        robot_state_set_ring_status("NO_LINK", NULL);
        s_coord.ring_ctx.state = RING_STATE_NO_LINK;
        s_coord.ring_ctx.last_rx_ms = -1;
        s_coord.ring_ctx.wait_until_ms = -1;
        s_coord.ring_ctx.last_value[0] = '\0';
    }
}

typedef struct {
    QueueHandle_t cam_queue;
    QueueHandle_t mic_cmd_queue;
} coordinator_args_t;

static void coordinator_task(void *arg) {
    coordinator_args_t *args = (coordinator_args_t *)arg;
    QueueHandle_t cam_queue = args->cam_queue;
    s_coord.mic_cmd_queue = args->mic_cmd_queue;
    free(args);

    camera_msg_t cam_msg;
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        const int64_t now_ms = esp_timer_get_time() / 1000;

        while (xQueueReceive(cam_queue, &cam_msg, 0) == pdTRUE) {
            if (cam_msg.msg.camera_id == CAMERA_ID_FLOOR) {
                handle_floor_message(&cam_msg, now_ms);
            } else if (cam_msg.msg.camera_id == CAMERA_ID_BEACON) {
                handle_beacon_message(&cam_msg);
            }
        }

        process_mic_commands(now_ms);

        bool mic_active = (s_coord.mic_cmd_expires_ms > 0 && now_ms < s_coord.mic_cmd_expires_ms);

        if (!mic_active) {
            process_ring_timers(now_ms);
        }

        odom_update(ODOM_UPDATE_MS / 1000.0f);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ODOM_UPDATE_MS));
    }
}

esp_err_t coordinator_start(QueueHandle_t cam_queue, QueueHandle_t mic_cmd_queue) {
    if (cam_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ring_context_init(&s_coord.ring_ctx);
    s_coord.beacon_last_seen_ms = -1;
    s_coord.beacon_class_id = 0;
    s_coord.mic_cmd_expires_ms = -1;
    s_coord.mic_cmd_queue = mic_cmd_queue;

    robot_state_set_ring_status("NO_LINK", NULL);
    stop_motors();

    coordinator_args_t *args = malloc(sizeof(coordinator_args_t));
    if (args == NULL) {
        return ESP_ERR_NO_MEM;
    }
    args->cam_queue = cam_queue;
    args->mic_cmd_queue = mic_cmd_queue;

    BaseType_t ok = xTaskCreate(coordinator_task, "coordinator", 4096, args, 6, NULL);
    if (ok != pdPASS) {
        free(args);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Coordinator task started");
    return ESP_OK;
}
