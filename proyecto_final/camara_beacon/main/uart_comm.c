#include "uart_comm.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"

#include "espnow_protocol.h"
#include "uart_protocol.h"

static const char *TAG = "uart_beacon";
static const uart_port_t s_uart_num = UART_NUM_1;
static const int UART_TX_GPIO = 3;
static const int UART_BAUD_RATE = 115200;

esp_err_t uart_init(void) {
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(s_uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(s_uart_num, UART_TX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_driver_install(s_uart_num, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "UART TX ready on GPIO%d (baud=%d)", UART_TX_GPIO, UART_BAUD_RATE);
    return ESP_OK;
}

esp_err_t uart_send_beacon_result(uint8_t class_id, uint8_t confidence) {
    espnow_camera_message_t msg = {
        .version = ESPNOW_PROTOCOL_VERSION,
        .camera_id = CAMERA_ID_BEACON,
        .data_type = DATA_TYPE_BEACON,
        .value = class_id,
        .confidence = confidence,
        .reserved = {0},
    };

    uint8_t frame[UART_FRAME_SIZE];
    size_t len = uart_frame_prepare(&msg, frame);

    int written = uart_write_bytes(s_uart_num, frame, len);
    if (written != len) {
        ESP_LOGW(TAG, "UART write failed: wrote %d of %d", written, len);
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}
