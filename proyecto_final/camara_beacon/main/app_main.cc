#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "camera_provider.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "identifier_detector.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "espnow_protocol.h"
#include "uart_comm.h"

namespace {

constexpr char kTag[] = "beacon_cam";

void LogResult(const IdentifierResult& result, int64_t elapsed_us) {
    ESP_LOGI(kTag,
             "class=%d (%s) detected=%s probability=%.1f%% margin=%d time=%lld us",
             result.class_id,
             IdentifierClassName(result.class_id),
             result.detected ? "yes" : "no",
             static_cast<double>(result.scores[kPresentClass] * 100.0f),
             result.raw_margin_over_absent,
             elapsed_us);
}

void WaitForNextInference(TickType_t cycle_start) {
    const TickType_t period = pdMS_TO_TICKS(CONFIG_IDENTIFIER_PERIOD_MS);
    const TickType_t elapsed = xTaskGetTickCount() - cycle_start;

    // Always block for at least one tick when inference misses its deadline so
    // IDLE0 can run and feed the task watchdog.
    vTaskDelay(elapsed < period ? period - elapsed : 1);
}

}  // namespace

extern "C" void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    IdentifierDetector detector;
    if (!detector.Begin()) {
        ESP_LOGE(kTag, "detector init failed");
        return;
    }

    if (!InitCameraProvider()) {
        ESP_LOGE(kTag, "camera init failed");
        return;
    }

    if (uart_init() != ESP_OK) {
        ESP_LOGE(kTag, "UART init failed");
        return;
    }

    static uint8_t image[kImageElementCount];
    while (true) {
        const TickType_t cycle_start = xTaskGetTickCount();

        if (!CaptureCameraImage(image)) {
            WaitForNextInference(cycle_start);
            continue;
        }

        IdentifierResult result;
        const int64_t start = esp_timer_get_time();
        const bool ok = detector.Run(image, &result);
        const int64_t elapsed = esp_timer_get_time() - start;

        if (ok) {
            LogResult(result, elapsed);
            uint8_t value = result.detected ? result.class_id : BEACON_ABSENT;
            uint8_t confidence = result.detected ? 100 : 0;
            uart_send_beacon_result(value, confidence);
        }

        WaitForNextInference(cycle_start);
    }
}
