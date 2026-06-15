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
    mic_command_t active_mic_cmd;
    bool mic_override_active;
    QueueHandle_t mic_cmd_queue;
} coordinator_t;

static coordinator_t s_coord = {0};

static void apply_mic_control(void) {
    switch (s_coord.active_mic_cmd) {
        case MIC_CMD_FULL_FORWARD:
            if (motor_get_speed() != MAX_SPEED) {
                motor_set_speed(MAX_SPEED);
            }
            move_forward();
            break;
        case MIC_CMD_BACKWARD:
            if (motor_get_speed() != MAX_SPEED) {
                motor_set_speed(MAX_SPEED);
            }
            move_robot("back");
            break;
        default:
            break;
    }
}

static void restore_ring_control(int64_t now_ms);

static void handle_mic_event(const mic_command_event_t *event, int64_t now_ms) {
    if (event->active) {
        s_coord.active_mic_cmd = event->cmd;
        s_coord.mic_override_active = true;
        ESP_LOGI(TAG, "MIC CMD active: %s",
                 event->cmd == MIC_CMD_FULL_FORWARD ? "FORWARD" : "BACKWARD");
        apply_mic_control();
    } else if (s_coord.mic_override_active &&
               s_coord.active_mic_cmd == event->cmd) {
        ESP_LOGI(TAG, "MIC CMD released");
        s_coord.active_mic_cmd = MIC_CMD_NONE;
        s_coord.mic_override_active = false;
        restore_ring_control(now_ms);
    }
}

static void process_mic_commands(int64_t now_ms) {
    if (s_coord.mic_cmd_queue == NULL) {
        return;
    }

    mic_command_event_t event;
    while (xQueueReceive(s_coord.mic_cmd_queue, &event, 0) == pdTRUE) {
        handle_mic_event(&event, now_ms);
    }
}

static void handle_floor_message(const camera_msg_t *cam_msg, int64_t now_ms) {
    const char *status = (cam_msg->msg.value == FLOOR_EDGE) ? "1" : "0";
    ring_context_update(&s_coord.ring_ctx, status, cam_msg->rx_time_ms);

    if (strcmp(status, "1") == 0) {
        if (s_coord.ring_ctx.state == RING_STATE_SAFE ||
            s_coord.ring_ctx.state == RING_STATE_NO_LINK) {
            ESP_LOGI(TAG, "EDGE detected: backing up for %d ms", RING_BACKUP_MS);
            motor_set_speed(RING_BACKUP_SPEED);
            move_robot("back");
            s_coord.ring_ctx.state = RING_STATE_EDGE_BACKING;
            s_coord.ring_ctx.wait_until_ms = now_ms + RING_BACKUP_MS;
            strlcpy(s_coord.ring_ctx.last_value, "1", sizeof(s_coord.ring_ctx.last_value));
        }
    } else if (strcmp(status, "0") == 0) {
        if (s_coord.ring_ctx.state == RING_STATE_EDGE_TURNING) {
            if (s_coord.ring_ctx.wait_until_ms > 0 &&
                now_ms >= s_coord.ring_ctx.wait_until_ms) {
                ESP_LOGI(TAG, "SAFE after minimum turn: recovery pause %d ms",
                         RING_RECOVERY_PAUSE_MS);
                stop_motors();
                s_coord.ring_ctx.state = RING_STATE_EDGE_RECOVERY;
                s_coord.ring_ctx.wait_until_ms = now_ms + RING_RECOVERY_PAUSE_MS;
            }
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
    if (s_coord.ring_ctx.state == RING_STATE_EDGE_BACKING &&
        s_coord.ring_ctx.wait_until_ms > 0 &&
        now_ms >= s_coord.ring_ctx.wait_until_ms) {
        ESP_LOGI(TAG, "Backup done: pausing %d ms before turn", RING_PRE_TURN_PAUSE_MS);
        stop_motors();
        s_coord.ring_ctx.state = RING_STATE_EDGE_WAIT_TURN;
        s_coord.ring_ctx.wait_until_ms = now_ms + RING_PRE_TURN_PAUSE_MS;
    }

    if (s_coord.ring_ctx.state == RING_STATE_EDGE_WAIT_TURN &&
        s_coord.ring_ctx.wait_until_ms > 0 &&
        now_ms >= s_coord.ring_ctx.wait_until_ms) {
        ESP_LOGI(TAG, "Pre-turn done: rotating left for at least %d ms", RING_MIN_TURN_MS);
        rotate_fast();
        s_coord.ring_ctx.state = RING_STATE_EDGE_TURNING;
        s_coord.ring_ctx.wait_until_ms = now_ms + RING_MIN_TURN_MS;
    }

    if (s_coord.ring_ctx.state == RING_STATE_EDGE_TURNING &&
        s_coord.ring_ctx.wait_until_ms > 0 &&
        now_ms >= s_coord.ring_ctx.wait_until_ms &&
        strcmp(s_coord.ring_ctx.last_value, "0") == 0) {
        ESP_LOGI(TAG, "Minimum turn done on SAFE floor: recovery pause %d ms",
                 RING_RECOVERY_PAUSE_MS);
        stop_motors();
        s_coord.ring_ctx.state = RING_STATE_EDGE_RECOVERY;
        s_coord.ring_ctx.wait_until_ms = now_ms + RING_RECOVERY_PAUSE_MS;
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

static void apply_ring_control(int64_t now_ms) {
    const char *action = ring_context_get_action(&s_coord.ring_ctx, now_ms);

    if (strcmp(action, "forward") == 0) {
        if (motor_get_speed() != RING_FOLLOW_SPEED) {
            motor_set_speed(RING_FOLLOW_SPEED);
        }
        move_forward();
    } else if (strcmp(action, "back") == 0) {
        if (motor_get_speed() != RING_BACKUP_SPEED) {
            motor_set_speed(RING_BACKUP_SPEED);
        }
        move_robot("back");
    } else if (strcmp(action, "rotate_left") == 0) {
        rotate_fast();
    } else {
        stop_motors();
    }
}

static void restore_ring_control(int64_t now_ms) {
    process_ring_timers(now_ms);

    const char *action = ring_context_get_action(&s_coord.ring_ctx, now_ms);
    ESP_LOGI(TAG, "MIC tone ended: restoring ring action=%s", action);
    apply_ring_control(now_ms);
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

        if (s_coord.mic_override_active) {
            apply_mic_control();
        } else {
            process_ring_timers(now_ms);
            apply_ring_control(now_ms);
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
    s_coord.active_mic_cmd = MIC_CMD_NONE;
    s_coord.mic_override_active = false;
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
