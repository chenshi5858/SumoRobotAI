#include "wifi_app.h"

#include <string.h>

#include "app_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "wifi_app";
static EventGroupHandle_t s_wifi_event_group;
static uint8_t s_primary_channel = ESPNOW_FALLBACK_CHANNEL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi started, connecting...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "HTTP endpoint: http://" IPSTR ":%d/move",
                 IP2STR(&event->ip_info.ip),
                 WEB_SERVER_PORT);
        ESP_LOGI(TAG, "WebSocket endpoint: ws://" IPSTR ":%d/ws",
                 IP2STR(&event->ip_info.ip),
                 WEB_SERVER_PORT);
    }
}

esp_err_t wifi_app_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

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

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGW(TAG, "WiFi timeout. ESP-NOW will use fallback channel %d", ESPNOW_FALLBACK_CHANNEL);
        ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_FALLBACK_CHANNEL, WIFI_SECOND_CHAN_NONE));
    }

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
    ESP_LOGI(TAG, "Current WiFi/ESP-NOW channel: %u", s_primary_channel);

    return ESP_OK;
}

uint8_t wifi_app_get_primary_channel(void) {
    return s_primary_channel;
}
