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
        .fb_count = 1,
        .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
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

static inline int abs_i32(int value) {
    return value < 0 ? -value : value;
}

static inline uint16_t clamp_u16(uint32_t value, uint16_t min_value, uint16_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return (uint16_t)value;
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

typedef struct {
    uint16_t gx;
    uint16_t gy;
} sobel_gradient_t;

static inline sobel_gradient_t sobel_gradient_at(const camera_fb_t *frame, uint32_t width, uint32_t x, uint32_t y) {
    const int tl = frame_luma_at(frame, width, x - 1U, y - 1U);
    const int tc = frame_luma_at(frame, width, x, y - 1U);
    const int tr = frame_luma_at(frame, width, x + 1U, y - 1U);
    const int ml = frame_luma_at(frame, width, x - 1U, y);
    const int mr = frame_luma_at(frame, width, x + 1U, y);
    const int bl = frame_luma_at(frame, width, x - 1U, y + 1U);
    const int bc = frame_luma_at(frame, width, x, y + 1U);
    const int br = frame_luma_at(frame, width, x + 1U, y + 1U);

    const int gx = -tl + tr - (2 * ml) + (2 * mr) - bl + br;
    const int gy = -tl - (2 * tc) - tr + bl + (2 * bc) + br;
    return (sobel_gradient_t) {
        .gx = (uint16_t)abs_i32(gx),
        .gy = (uint16_t)abs_i32(gy),
    };
}

bool detect_ring_border(const camera_fb_t *frame, ring_detection_metrics_t *metrics) {
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

    const uint32_t start_y = BORDER_ROI_Y_START < height ? BORDER_ROI_Y_START : height;
    const uint32_t end_y = BORDER_ROI_Y_END < height ? BORDER_ROI_Y_END : height;
    const uint32_t start_x = BORDER_ROI_X_START < width ? BORDER_ROI_X_START : width;
    const uint32_t end_x = BORDER_ROI_X_END < width ? BORDER_ROI_X_END : width;

    if (start_y >= end_y || start_x >= end_x) {
        ESP_LOGW(TAG, "Invalid ROI: (%" PRIu32 ",%" PRIu32 ")-(%" PRIu32 ",%" PRIu32 ")",
                 start_x,
                 start_y,
                 end_x,
                 end_y);
        return false;
    }

    uint32_t total_count = 0;
    uint64_t edge_gradient_sum = 0;
    uint32_t edge_rows = 0;

    if ((end_y - start_y) >= 3U && (end_x - start_x) >= 3U) {
        for (uint32_t y = start_y + 1U; (y + 1U) < end_y; y++) {
            for (uint32_t x = start_x + 1U; (x + 1U) < end_x; x++) {
                const sobel_gradient_t gradient = sobel_gradient_at(frame, width, x, y);
                edge_gradient_sum += gradient.gy;
                total_count++;
            }
        }
    }

    const uint16_t edge_threshold = total_count > 0
        ? clamp_u16((uint32_t)(edge_gradient_sum / total_count) + BORDER_EDGE_GRADIENT_OFFSET,
                    BORDER_EDGE_GRADIENT_MIN,
                    BORDER_EDGE_GRADIENT_MAX)
        : BORDER_EDGE_GRADIENT_MAX;
    uint32_t edge_count = 0;
    uint32_t best_row_pixels = 0;

    if ((end_y - start_y) >= 3U && (end_x - start_x) >= 3U) {
        for (uint32_t y = start_y + 1U; (y + 1U) < end_y; y++) {
            uint32_t row_edge_count = 0;

            for (uint32_t x = start_x + 1U; (x + 1U) < end_x; x++) {
                const sobel_gradient_t gradient = sobel_gradient_at(frame, width, x, y);

                if (gradient.gy >= edge_threshold &&
                    gradient.gy >= (uint16_t)(gradient.gx + BORDER_EDGE_HORIZONTAL_DOMINANCE)) {
                    edge_count++;
                    row_edge_count++;
                }
            }

            if (row_edge_count > best_row_pixels) {
                best_row_pixels = row_edge_count;
            }
            if (row_edge_count >= BORDER_EDGE_ROW_MIN_PIXELS) {
                edge_rows++;
            }
        }
    }

    const bool edge_trigger =
        total_count > 0 &&
        edge_rows >= BORDER_EDGE_MIN_ROWS &&
        edge_rows <= BORDER_EDGE_MAX_ROWS &&
        best_row_pixels >= BORDER_EDGE_ROW_MIN_PIXELS &&
        (edge_count * 100U) <= (total_count * BORDER_EDGE_MAX_PERCENT) &&
        (edge_count * 100U) >= (total_count * BORDER_EDGE_MIN_PERCENT);

    if (metrics != NULL) {
        metrics->total_pixels = total_count;
        metrics->edge_pixels = edge_count;
        metrics->edge_rows = edge_rows;
        metrics->best_row_pixels = best_row_pixels;
        metrics->edge_threshold = edge_threshold;
        metrics->edge_trigger = edge_trigger;
    }

    return edge_trigger;
}
