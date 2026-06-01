#include "esp_log.h"
#include "esp_err.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

static const char* TAG = "EdgeDetect";

static constexpr int kFrameW = 96;
static constexpr int kFrameH = 96;

static constexpr int kBandStart  = 72;
static constexpr int kBandEnd    = 96;
static constexpr int kRoiXStart  = 24;
static constexpr int kRoiXEnd    = 72;

static constexpr int kWhitePercentThreshold = 15;
static constexpr int kDebounceCount        = 3;

#define UART_PORT_NUM    UART_NUM_2
#define UART_TX_GPIO     14
#define UART_BAUD        115200

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
    cfg.pixel_format  = PIXFORMAT_RGB565;
    cfg.frame_size    = FRAMESIZE_96X96;
    cfg.fb_count      = 1;
    cfg.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;
    cfg.fb_location   = CAMERA_FB_IN_DRAM;

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: 0x%x", err);
        return err;
    }

    sensor_t* s = esp_camera_sensor_get();
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);

    ESP_LOGI(TAG, "OV2640 ready – 96x96 RGB565");
    return ESP_OK;
}

static void uart_init(void) {
    uart_config_t uart_conf = {};
    uart_conf.baud_rate = UART_BAUD;
    uart_conf.data_bits = UART_DATA_8_BITS;
    uart_conf.parity    = UART_PARITY_DISABLE;
    uart_conf.stop_bits = UART_STOP_BITS_1;
    uart_conf.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_param_config(UART_PORT_NUM, &uart_conf);
    uart_set_pin(UART_PORT_NUM, UART_TX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, 256, 0, 0, NULL, 0);
}

static void pixel_rgb565(const uint8_t* buf, int idx, uint8_t* r, uint8_t* g, uint8_t* b) {
    uint16_t p = (buf[idx * 2 + 1] << 8) | buf[idx * 2];
    uint8_t r5 = (p >> 11) & 0x1F;
    uint8_t g6 = (p >> 5)  & 0x3F;
    uint8_t b5 =  p        & 0x1F;
    *r = (r5 * 255 + 15) / 31;
    *g = (g6 * 255 + 31) / 63;
    *b = (b5 * 255 + 0) / 31;
}

static bool is_white(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t lum = (r * 77 + g * 150 + b * 29) >> 8;
    return lum > 180;
}

static int count_white_pixels(const uint8_t* fb_buf) {
    int white = 0;
    for (int y = kBandStart; y < kBandEnd; y++) {
        for (int x = kRoiXStart; x < kRoiXEnd; x++) {
            int idx = y * kFrameW + x;
            uint8_t r, g, b;
            pixel_rgb565(fb_buf, idx, &r, &g, &b);
            if (is_white(r, g, b)) {
                white++;
            }
        }
    }
    return white;
}

static void detection_task(void*) {
    int edge_count = 0;
    bool last_sent_edge = false;
    for (;;) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Frame capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        int total      = (kBandEnd - kBandStart) * (kRoiXEnd - kRoiXStart);
        int white      = count_white_pixels(fb->buf);
        int white_pct  = (white * 100) / total;
        bool edge      = (white_pct >= kWhitePercentThreshold);

        if (edge) {
            if (edge_count < kDebounceCount) edge_count++;
        } else {
            if (edge_count > 0) edge_count--;
        }

        bool debounced = (edge_count >= kDebounceCount);

        if (debounced != last_sent_edge) {
            last_sent_edge = debounced;
            uart_write_bytes(UART_PORT_NUM, debounced ? "E" : "S", 1);
        }

        ESP_LOGI(TAG, "Band white=%d%% %s", white_pct, debounced ? "EDGE" : "SAFE");

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== Edge Detection – ESP32-CAM ===");

    ESP_ERROR_CHECK(camera_init());
    uart_init();

    xTaskCreatePinnedToCore(
        detection_task,
        "detection",
        4096,
        nullptr,
        5,
        nullptr,
        1
    );
}