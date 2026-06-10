#include "camera_app.h"

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
        sensor->set_brightness(sensor, 0);
        sensor->set_contrast(sensor, 0);
        sensor->set_whitebal(sensor, 1);
        sensor->set_awb_gain(sensor, 1);
    }

    ESP_LOGI(TAG, "Camera ready: 96x96 RGB565, fb_count=%d", config.fb_count);
    return ESP_OK;
}

static inline void rgb565_split(const uint8_t *p, int *r, int *g, int *b) {
    uint16_t px = (uint16_t)((p[0] << 8) | p[1]);
    *r = ((px >> 11) & 0x1F) << 3;
    *g = ((px >> 5)  & 0x3F) << 2;
    *b = ( px        & 0x1F) << 3;
}

/*
 * Detecta borde negro por umbral de brillo.
 * Cuenta pixeles donde (R+G+B)/3 <= BLACK_THRESHOLD.
 * Si el porcentaje supera BLACK_MIN_PERCENT, declara borde.
 */
bool detect_edge(const camera_fb_t *frame, edge_metrics_t *metrics) {
    if (metrics != NULL) {
        memset(metrics, 0, sizeof(*metrics));
    }

    if (frame == NULL || frame->buf == NULL || frame->width == 0 || frame->height == 0) {
        ESP_LOGW(TAG, "Invalid frame received");
        return false;
    }

    if (frame->format != PIXFORMAT_RGB565) {
        ESP_LOGW(TAG, "Unsupported frame format: %d", frame->format);
        return false;
    }

    const uint32_t width = frame->width;
    const uint32_t height = frame->height;
    const uint8_t *buf = frame->buf;

    uint32_t black = 0;
    const uint32_t total = width * height;

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t *row = &buf[(size_t)y * width * 2];
        for (uint32_t x = 0; x < width; x++) {
            int r, g, b;
            rgb565_split(&row[x * 2], &r, &g, &b);
            if ((r + g + b) / 3 <= BLACK_THRESHOLD) {
                black++;
            }
        }
    }

    const uint32_t black_percent = total > 0 ? (black * 100U) / total : 0;
    const bool detected = black_percent >= BLACK_MIN_PERCENT;

    if (metrics != NULL) {
        metrics->total_pixels = total;
        metrics->black_pixels = black;
        metrics->black_percent = black_percent;
        metrics->edge_detected = detected;
    }

    return detected;
}
