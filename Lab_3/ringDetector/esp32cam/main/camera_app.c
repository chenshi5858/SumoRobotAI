#include "camera_app.h"

#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "camera_app";

esp_err_t init_camera(void) {
    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_GRAYSCALE,
        .frame_size = FRAMESIZE_96X96,
        .jpeg_quality = 12,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return err;
    }

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor != NULL) {
        sensor->set_framesize(sensor, FRAMESIZE_96X96);
        sensor->set_pixformat(sensor, PIXFORMAT_GRAYSCALE);
    }

    ESP_LOGI(TAG, "Camera ready: 96x96 grayscale, fb_count=%d", config.fb_count);
    return ESP_OK;
}

bool detect_white_region(const camera_fb_t *frame, uint32_t *white_pixels, uint32_t *total_pixels) {
    if (white_pixels != NULL) {
        *white_pixels = 0;
    }
    if (total_pixels != NULL) {
        *total_pixels = 0;
    }

    if (frame == NULL || frame->buf == NULL || frame->width == 0 || frame->height == 0) {
        ESP_LOGW(TAG, "Invalid frame received");
        return false;
    }

    const uint32_t width = frame->width;
    const uint32_t height = frame->height;
    const uint32_t region_height = (height * WHITE_REGION_PERCENT) / 100;
    const uint32_t start_y = height - region_height;
    uint32_t white_count = 0;
    uint32_t total_count = 0;

    /*
     * PIXFORMAT_GRAYSCALE entrega un byte por pixel.
     * Solo se analiza el rectangulo inferior para detectar el borde blanco.
     */
    for (uint32_t y = start_y; y < height; y++) {
        const uint8_t *row = frame->buf + (y * width);
        for (uint32_t x = 0; x < width; x++) {
            if (row[x] > WHITE_PIXEL_THRESHOLD) {
                white_count++;
            }
            total_count++;
        }
    }

    if (white_pixels != NULL) {
        *white_pixels = white_count;
    }
    if (total_pixels != NULL) {
        *total_pixels = total_count;
    }

    return (white_count * 100U) > (total_count * WHITE_MIN_PERCENT);
}
