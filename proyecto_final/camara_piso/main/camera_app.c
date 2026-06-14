#include "camera_app.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "esp_log.h"

static const char *TAG = "camera_app";

static uint8_t s_lum_buf[CAMERA_FRAME_WIDTH * CAMERA_FRAME_HEIGHT];

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

static void frame_to_luminance(const uint8_t *buf) {
    for (int i = 0; i < CAMERA_FRAME_WIDTH * CAMERA_FRAME_HEIGHT; i++) {
        uint16_t p = (buf[i * 2 + 1] << 8) | buf[i * 2];
        uint8_t r5 = (p >> 11) & 0x1F;
        uint8_t g6 = (p >> 5)  & 0x3F;
        uint8_t b5 =  p        & 0x1F;
        uint8_t r = (r5 * 255 + 15) / 31;
        uint8_t g = (g6 * 255 + 31) / 63;
        uint8_t b = (b5 * 255 +  0) / 31;
        s_lum_buf[i] = (r * 77 + g * 150 + b * 29) >> 8;
    }
}

static int32_t sobel_gy_at(uint32_t x, uint32_t y) {
    const int32_t tl = s_lum_buf[(y - 1) * CAMERA_FRAME_WIDTH + (x - 1)];
    const int32_t tc = s_lum_buf[(y - 1) * CAMERA_FRAME_WIDTH +  x];
    const int32_t tr = s_lum_buf[(y - 1) * CAMERA_FRAME_WIDTH + (x + 1)];
    const int32_t bl = s_lum_buf[(y + 1) * CAMERA_FRAME_WIDTH + (x - 1)];
    const int32_t bc = s_lum_buf[(y + 1) * CAMERA_FRAME_WIDTH +  x];
    const int32_t br = s_lum_buf[(y + 1) * CAMERA_FRAME_WIDTH + (x + 1)];

    return -tl - 2 * tc - tr + bl + 2 * bc + br;
}

bool detect_edge(const camera_fb_t *frame, edge_metrics_t *metrics) {
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
        ESP_LOGW(TAG, "Unsupported frame format/size: format=%d len=%u", frame->format, (unsigned)frame->len);
        return false;
    }

    frame_to_luminance(frame->buf);

    const uint32_t start_y = EDGE_ROI_Y_START < height ? EDGE_ROI_Y_START : height;
    const uint32_t end_y = EDGE_ROI_Y_END < height ? EDGE_ROI_Y_END : height;
    const uint32_t start_x = EDGE_ROI_X_START < width ? EDGE_ROI_X_START : width;
    const uint32_t end_x = EDGE_ROI_X_END < width ? EDGE_ROI_X_END : width;

    if (start_y + 1 >= end_y || start_x + 1 >= end_x) {
        ESP_LOGW(TAG, "Invalid ROI: (%" PRIu32 ",%" PRIu32 ")-(%" PRIu32 ",%" PRIu32 ")",
                 start_x, start_y, end_x, end_y);
        return false;
    }

    uint32_t total = 0;
    uint32_t edges = 0;
    uint32_t active_rows = 0;
    uint32_t best_row_edges = 0;

    for (uint32_t y = start_y + 1; y < end_y - 1; y++) {
        uint32_t row_edges = 0;
        for (uint32_t x = start_x + 1; x < end_x - 1; x++) {
            int32_t gy = sobel_gy_at(x, y);
            total++;
            if (gy < -EDGE_DARK_THRESHOLD) {
                edges++;
                row_edges++;
            }
        }
        if (row_edges >= EDGE_MIN_ROW_PIXELS) {
            active_rows++;
        }
        if (row_edges > best_row_edges) {
            best_row_edges = row_edges;
        }
    }

    const uint32_t edge_percent = total > 0 ? (edges * 100U) / total : 0;
    const bool detected = edge_percent >= EDGE_MIN_PERCENT && active_rows >= EDGE_MIN_ACTIVE_ROWS;

    if (metrics != NULL) {
        metrics->total_pixels = total;
        metrics->edge_pixels = edges;
        metrics->edge_percent = edge_percent;
        metrics->active_rows = active_rows;
        metrics->best_row_edges = best_row_edges;
        metrics->edge_detected = detected;
    }

    return detected;
}

void debug_print_edge_map(void) {
    const uint32_t start_y = EDGE_ROI_Y_START;
    const uint32_t end_y = EDGE_ROI_Y_END;
    const uint32_t start_x = EDGE_ROI_X_START;
    const uint32_t end_x = EDGE_ROI_X_END;

    ESP_LOGI(TAG, "--- Edge map (Gy < -%d, row_min=%d, active_rows_min=%d, step=4) ---",
             EDGE_DARK_THRESHOLD,
             EDGE_MIN_ROW_PIXELS,
             EDGE_MIN_ACTIVE_ROWS);
    for (uint32_t y = start_y + 1; y < end_y - 1; y += 4) {
        char line[64];
        size_t pos = 0;
        for (uint32_t x = start_x + 1; x < end_x - 1; x += 2) {
            int32_t gy = sobel_gy_at(x, y);
            line[pos++] = gy < -EDGE_DARK_THRESHOLD ? '#' : '.';
        }
        line[pos] = '\0';
        ESP_LOGI(TAG, "%s", line);
    }
    ESP_LOGI(TAG, "--- end map ---");
}
