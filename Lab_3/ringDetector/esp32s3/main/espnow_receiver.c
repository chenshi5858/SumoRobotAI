#include "espnow_receiver.h"

#include <stdbool.h>
#include <string.h>

#include "app_config.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "robot_state.h"

static const char *TAG = "espnow_rx";
static QueueHandle_t s_status_queue = NULL;
static uint8_t s_primary_channel = ESPNOW_CHANNEL;
#if !ACCEPT_ANY_CAMERA_MAC
static const uint8_t s_allowed_camera_mac[ESP_NOW_ETH_ALEN] = CAMERA_MAC_BYTES;
#endif
static bool is_allowed_camera(const uint8_t *mac) {
    if (mac == NULL) {
        return false;
    }

#if ACCEPT_ANY_CAMERA_MAC
    return true;
#else
    return memcmp(mac, s_allowed_camera_mac, ESP_NOW_ETH_ALEN) == 0;
#endif
}

static esp_err_t init_wifi_for_espnow(void) {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        return err;
    }

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    wifi_second_chan_t second_channel = WIFI_SECOND_CHAN_NONE;
    ESP_ERROR_CHECK(esp_wifi_get_channel(&s_primary_channel, &second_channel));

    uint8_t local_mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(local_mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG,
             "ESP32-S3 STA MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             local_mac[0],
             local_mac[1],
             local_mac[2],
             local_mac[3],
             local_mac[4],
             local_mac[5]);
    ESP_LOGI(TAG, "ESP-NOW channel: %u", s_primary_channel);

    return ESP_OK;
}

static bool normalize_status(const uint8_t *data, int data_len, char *out, size_t out_len) {
    if (data == NULL || data_len <= 0 || out == NULL || out_len == 0) {
        return false;
    }

    const size_t copy_len = (data_len < (int)(out_len - 1)) ? (size_t)data_len : (out_len - 1);
    memcpy(out, data, copy_len);
    out[copy_len] = '\0';

    if (strcmp(out, "SAFE") == 0 || strcmp(out, "WHITE") == 0) {
        return true;
    }

    ESP_LOGW(TAG, "Unknown ESP-NOW payload: %s", out);
    return false;
}

static void handle_received_packet(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (!is_allowed_camera(mac_addr)) {
        ESP_LOGW(TAG,
                 "Ignored packet from %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0],
                 mac_addr[1],
                 mac_addr[2],
                 mac_addr[3],
                 mac_addr[4],
                 mac_addr[5]);
        return;
    }

    espnow_status_msg_t msg = {0};
    if (!normalize_status(data, data_len, msg.status, sizeof(msg.status))) {
        return;
    }

    memcpy(msg.source_mac, mac_addr, sizeof(msg.source_mac));
    msg.rx_time_ms = esp_timer_get_time() / 1000;
    robot_state_set_ring_status(msg.status, msg.source_mac);

    if (s_status_queue == NULL) {
        ESP_LOGW(TAG, "Status queue not ready");
        return;
    }

    if (xQueueSend(s_status_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Status queue full, dropping %s", msg.status);
    } else {
        ESP_LOGI(TAG,
                 "ESP-NOW RX %s from %02X:%02X:%02X:%02X:%02X:%02X",
                 msg.status,
                 msg.source_mac[0],
                 msg.source_mac[1],
                 msg.source_mac[2],
                 msg.source_mac[3],
                 msg.source_mac[4],
                 msg.source_mac[5]);
    }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void on_espnow_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
    if (recv_info == NULL || recv_info->src_addr == NULL) {
        ESP_LOGW(TAG, "ESP-NOW packet without source info");
        return;
    }
    handle_received_packet(recv_info->src_addr, data, data_len);
}
#else
static void on_espnow_recv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (mac_addr == NULL) {
        ESP_LOGW(TAG, "ESP-NOW packet without source MAC");
        return;
    }
    handle_received_packet(mac_addr, data, data_len);
}
#endif

esp_err_t espnow_receiver_init(QueueHandle_t status_queue) {
    if (status_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_status_queue = status_queue;

    esp_err_t err = init_wifi_for_espnow();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi radio init for ESP-NOW failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_now_register_recv_cb(on_espnow_recv);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "register recv callback failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "ESP-NOW receiver ready on channel %u", s_primary_channel);
#if !ACCEPT_ANY_CAMERA_MAC
    ESP_LOGI(TAG,
             "Accepting camera MAC %02X:%02X:%02X:%02X:%02X:%02X",
             s_allowed_camera_mac[0],
             s_allowed_camera_mac[1],
             s_allowed_camera_mac[2],
             s_allowed_camera_mac[3],
             s_allowed_camera_mac[4],
             s_allowed_camera_mac[5]);
#else
    ESP_LOGI(TAG, "Accepting ESP-NOW packets from any camera MAC");
#endif

    return ESP_OK;
}
