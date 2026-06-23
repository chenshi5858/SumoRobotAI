#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "driver/uart.h"

#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK     0
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0       5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

static const char *TAG = "cam_capture";

static esp_err_t camera_init(void) {
    camera_config_t cfg = {};
    cfg.ledc_channel  = LEDC_CHANNEL_0;
    cfg.ledc_timer    = LEDC_TIMER_0;
    cfg.pin_d0        = CAM_PIN_D0;
    cfg.pin_d1        = CAM_PIN_D1;
    cfg.pin_d2        = CAM_PIN_D2;
    cfg.pin_d3        = CAM_PIN_D3;
    cfg.pin_d4        = CAM_PIN_D4;
    cfg.pin_d5        = CAM_PIN_D5;
    cfg.pin_d6        = CAM_PIN_D6;
    cfg.pin_d7        = CAM_PIN_D7;
    cfg.pin_xclk      = CAM_PIN_XCLK;
    cfg.pin_pclk      = CAM_PIN_PCLK;
    cfg.pin_vsync     = CAM_PIN_VSYNC;
    cfg.pin_href      = CAM_PIN_HREF;
    cfg.pin_sccb_sda  = CAM_PIN_SIOD;
    cfg.pin_sccb_scl  = CAM_PIN_SIOC;
    cfg.pin_pwdn      = CAM_PIN_PWDN;
    cfg.pin_reset     = CAM_PIN_RESET;
    cfg.xclk_freq_hz  = 20000000;
    cfg.pixel_format  = PIXFORMAT_GRAYSCALE;
    cfg.frame_size    = FRAMESIZE_96X96;
    cfg.fb_count      = 2;
    cfg.grab_mode     = CAMERA_GRAB_LATEST;
    cfg.fb_location   = CAMERA_FB_IN_DRAM;

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_brightness(s, 0);
    s->set_contrast(s, 1);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);

    ESP_LOGI(TAG, "OV2640 ready - 96x96 GRAYSCALE");
    return ESP_OK;
}

static void send_image(const uint8_t *buf, size_t len) {
    printf("\nIMG_START %zu\n", len);
    for (size_t i = 0; i < len; i++) {
        putchar(buf[i]);
    }
    printf("\nIMG_END\n");
    fflush(stdout);
}

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-CAM Capture Tool ===");
    ESP_ERROR_CHECK(camera_init());

    ESP_LOGI(TAG, "Send 'c' via serial to capture an image");

    while (1) {
        int ch = getchar();
        if (ch == 'c' || ch == 'C') {
            ESP_LOGI(TAG, "Capturing...");
            camera_fb_t *pic = esp_camera_fb_get();
            if (!pic) {
                ESP_LOGE(TAG, "Failed to capture");
                continue;
            }
            ESP_LOGI(TAG, "Captured %zu bytes", pic->len);
            send_image(pic->buf, pic->len);
            esp_camera_fb_return(pic);
            ESP_LOGI(TAG, "Sent. Ready for next capture.");
        }
    }
}
