#include "camera_app.h"

#include <inttypes.h>
#include <string.h>

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

        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_RGB565,
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
        sensor->set_pixformat(sensor, PIXFORMAT_RGB565);
    }

    ESP_LOGI(TAG, "Camera ready: 96x96 RGB565, fb_count=%d", config.fb_count);
    return ESP_OK;
}

static inline uint8_t rgb565_luma(const uint8_t *buf, uint32_t index) {
    const uint32_t byte_index = index * 2U;
    const uint16_t pixel = ((uint16_t)buf[byte_index + 1U] << 8U) | buf[byte_index];
    const uint8_t r5 = (pixel >> 11) & 0x1F;
    const uint8_t g6 = (pixel >> 5) & 0x3F;
    const uint8_t b5 = pixel & 0x1F;
    const uint8_t r = (uint8_t)((r5 * 255U + 15U) / 31U);
    const uint8_t g = (uint8_t)((g6 * 255U + 31U) / 63U);
    const uint8_t b = (uint8_t)((b5 * 255U + 15U) / 31U);
    return (uint8_t)(((uint16_t)r * 77U + (uint16_t)g * 150U + (uint16_t)b * 29U) >> 8U);
}

static inline uint8_t frame_luma_at(const camera_fb_t *frame, uint32_t width, uint32_t x, uint32_t y) {
    return rgb565_luma(frame->buf, (y * width) + x);
}

bool detect_black_line(const camera_fb_t *frame, black_line_metrics_t *metrics) {
    if (metrics != NULL) {
        memset(metrics, 0, sizeof(*metrics));
    }

    if (frame == NULL || frame->buf == NULL || frame->width == 0 || frame->height == 0) {
        ESP_LOGW(TAG, "Invalid frame received");
        return false;
    }

    const uint32_t width = frame->width;
    const uint32_t height = frame->height;

    if (frame->format != PIXFORMAT_RGB565 || frame->len < (width * height * 2U)) {
        ESP_LOGW(TAG, "Unsupported frame format/size: format=%d len=%u width=%" PRIu32 " height=%" PRIu32,
                 frame->format,
                 (unsigned)frame->len,
                 width,
                 height);
        return false;
    }

    const uint32_t start_y = ROI_Y_START < height ? ROI_Y_START : height;
    const uint32_t end_y = ROI_Y_END < height ? ROI_Y_END : height;
    const uint32_t start_x = ROI_X_START < width ? ROI_X_START : width;
    const uint32_t end_x = ROI_X_END < width ? ROI_X_END : width;

    if (start_y >= end_y || start_x >= end_x) {
        ESP_LOGW(TAG, "Invalid ROI: (%" PRIu32 ",%" PRIu32 ")-(%" PRIu32 ",%" PRIu32 ")",
                 start_x, start_y, end_x, end_y);
        return false;
    }

    const uint32_t luma_threshold = BLACK_LUMA_THRESHOLD;
    uint32_t total_pixels = 0;
    uint32_t black_pixels = 0;

    for (uint32_t y = start_y; y < end_y; y++) {
        for (uint32_t x = start_x; x < end_x; x++) {
            const uint8_t luma = frame_luma_at(frame, width, x, y);
            total_pixels++;
            if (luma < luma_threshold) {
                black_pixels++;
            }
        }
    }

    const uint32_t black_percent = total_pixels > 0
        ? (black_pixels * 100U) / total_pixels
        : 0;

    const bool detected = black_percent >= BLACK_MIN_PERCENT;

    if (metrics != NULL) {
        metrics->total_pixels = total_pixels;
        metrics->black_pixels = black_pixels;
        metrics->black_percent = black_percent;
        metrics->black_detected = detected;
    }

    return detected;
}
