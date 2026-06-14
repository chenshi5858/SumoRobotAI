#include "uart_receiver.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "main_config.h"
#include "robot_state.h"
#include "uart_protocol.h"

static const char *TAG = "uart_rx";

typedef struct {
    uart_port_t uart_num;
    int rx_gpio;
    uint8_t camera_id;
    QueueHandle_t out_queue;
} uart_rx_task_arg_t;

static void uart_rx_task(void *arg) {
    uart_rx_task_arg_t *cfg = (uart_rx_task_arg_t *)arg;
    uint8_t buf[UART_FRAME_SIZE];
    size_t pos = 0;
    int64_t last_rx_ms = -1;

    while (1) {
        uint8_t byte;
        int len = uart_read_bytes(cfg->uart_num, &byte, 1, pdMS_TO_TICKS(50));
        if (len <= 0) {
            continue;
        }

        if (byte == UART_MSG_START_BYTE) {
            pos = 1;
            buf[0] = byte;

            while (pos < UART_FRAME_SIZE) {
                len = uart_read_bytes(cfg->uart_num, buf + pos, UART_FRAME_SIZE - pos, pdMS_TO_TICKS(10));
                if (len > 0) {
                    pos += len;
                } else {
                    break;
                }
            }

            if (pos == UART_FRAME_SIZE) {
                espnow_camera_message_t msg;
                if (uart_frame_parse(buf, UART_FRAME_SIZE, &msg)) {
                    if (msg.camera_id == cfg->camera_id) {
                        last_rx_ms = esp_timer_get_time() / 1000;

                        if (msg.camera_id == CAMERA_ID_FLOOR) {
                            const char *s = (msg.value == FLOOR_EDGE) ? "1" : "0";
                            robot_state_set_ring_status(s, NULL);
                        } else if (msg.camera_id == CAMERA_ID_BEACON) {
                            robot_state_set_beacon(msg.value);
                        }

                        if (cfg->out_queue != NULL) {
                            camera_msg_t out = {0};
                            out.msg = msg;
                            out.rx_time_ms = last_rx_ms;
                            if (xQueueSend(cfg->out_queue, &out, 0) != pdTRUE) {
                                ESP_LOGW(TAG, "Queue full, dropping cam=%u val=%u",
                                         msg.camera_id, msg.value);
                            }
                        }
                    }
                }
            }

            pos = 0;
        }
    }
}

esp_err_t uart_receiver_start(const uart_cam_config_t *config, QueueHandle_t out_queue) {
    if (config == NULL || out_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(config->uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART%d param config failed: %s", config->uart_num, esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(config->uart_num, UART_PIN_NO_CHANGE, config->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART%d set pin failed: %s", config->uart_num, esp_err_to_name(err));
        return err;
    }

    err = uart_driver_install(config->uart_num, 512, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART%d driver install failed: %s", config->uart_num, esp_err_to_name(err));
        return err;
    }

    uart_rx_task_arg_t *arg = malloc(sizeof(uart_rx_task_arg_t));
    if (arg == NULL) {
        return ESP_ERR_NO_MEM;
    }

    arg->uart_num = config->uart_num;
    arg->rx_gpio = config->rx_gpio;
    arg->camera_id = config->camera_id;
    arg->out_queue = out_queue;

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "uart_rx_%u", config->camera_id);

    BaseType_t ok = xTaskCreate(uart_rx_task, task_name, 4096, arg, 5, NULL);
    if (ok != pdPASS) {
        free(arg);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UART%d RX ready on GPIO%d for camera ID=%u",
             config->uart_num, config->rx_gpio, config->camera_id);
    return ESP_OK;
}
