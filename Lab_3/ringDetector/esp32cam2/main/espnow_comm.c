#include "espnow_comm.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_netif.h"

static const char *TAG = "espnow_tx";
static const uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = ESPNOW_PEER_MAC_BYTES;

static void format_mac(const uint8_t *mac, char *out, size_t out_len) {
    snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
static void on_espnow_send(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
}
#else
static void on_espnow_send(const uint8_t *mac_addr, esp_now_send_status_t status) {
}
#endif

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

    uint8_t local_mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(local_mac, ESP_MAC_WIFI_STA));

    char mac_text[18];
    format_mac(local_mac, mac_text, sizeof(mac_text));
    ESP_LOGI(TAG, "Local ESP32-CAM STA MAC: %s", mac_text);
    ESP_LOGI(TAG, "ESP-NOW WiFi channel: %d", ESPNOW_CHANNEL);

    return ESP_OK;
}

esp_err_t init_espnow(void) {
    esp_err_t err = init_wifi_for_espnow();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init for ESP-NOW failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(esp_now_register_send_cb(on_espnow_send));

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "esp_now_add_peer failed: %s", esp_err_to_name(err));
        return err;
    }

    char peer_text[18];
    format_mac(s_peer_mac, peer_text, sizeof(peer_text));
    ESP_LOGI(TAG, "ESP-NOW transmitter ready, peer=%s", peer_text);

    return ESP_OK;
}

esp_err_t send_status(const char *status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t len = strlen(status) + 1;
    if (len > ESP_NOW_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = esp_now_send(s_peer_mac, (const uint8_t *)status, len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "send_status(%s) failed: %s", status, esp_err_to_name(err));
    }

    return err;
}
