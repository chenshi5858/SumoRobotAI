#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "camera_provider.h"
#include "driver/gpio.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "identifier_detector.h"
#include "sdkconfig.h"
#include "static_image_data.h"

namespace {

constexpr char kTag[] = "identifier_app";

void ConfigureLed() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << CONFIG_IDENTIFIER_LED_GPIO;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_IDENTIFIER_LED_GPIO), 0);
}

void SetLed(bool on) {
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_IDENTIFIER_LED_GPIO), on ? 1 : 0);
}

void LogResult(const IdentifierResult& result, int64_t elapsed_us) {
    ESP_LOGI(kTag,
             "class=%d (%s) detected=%s margin=%d time=%lld us scores=[%.3f %.3f %.3f %.3f] raw=[%d %d %d %d]",
             result.class_id,
             IdentifierClassName(result.class_id),
             result.detected ? "yes" : "no",
             result.raw_margin_over_absent,
             elapsed_us,
             static_cast<double>(result.scores[0]),
             static_cast<double>(result.scores[1]),
             static_cast<double>(result.scores[2]),
             static_cast<double>(result.scores[3]),
             result.raw_scores[0],
             result.raw_scores[1],
             result.raw_scores[2],
             result.raw_scores[3]);
}

#if !CONFIG_IDENTIFIER_USE_CAMERA
IdentifierDetector* g_detector = nullptr;

int DetectImageCliHandler(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: detect_image <index>\n");
        return 0;
    }
    if (g_detector == nullptr) {
        ESP_LOGE(kTag, "detector not ready");
        return -1;
    }

    const int index = atoi(argv[1]);
    if (index < 0 || index >= static_cast<int>(g_static_image_count)) {
        ESP_LOGE(kTag, "index out of range (0-%d)", static_cast<int>(g_static_image_count) - 1);
        return -1;
    }

    IdentifierResult result;
    const int64_t start = esp_timer_get_time();
    const bool ok = g_detector->Run(g_static_images[index], &result);
    const int64_t elapsed = esp_timer_get_time() - start;

    if (ok) {
        SetLed(result.detected);
        LogResult(result, elapsed);
    } else {
        SetLed(false);
        ESP_LOGE(kTag, "inference failed");
    }

    return 0;
}

int ListImagesCliHandler(int argc, char** argv) {
    (void)argc;
    (void)argv;
    printf("\nstatic images: %d\n", static_cast<int>(g_static_image_count));
    for (size_t i = 0; i < g_static_image_count; ++i) {
        printf("%d: %s\n", static_cast<int>(i), g_static_image_names[i]);
    }
    return 0;
}

void StartCli(IdentifierDetector* detector) {
    g_detector = detector;

    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    esp_console_register_help_command();
    esp_console_cmd_t detect_cmd = {};
    detect_cmd.command = "detect_image";
    detect_cmd.help = "detect_image <index>";
    detect_cmd.func = DetectImageCliHandler;

    esp_console_cmd_t list_cmd = {};
    list_cmd.command = "list_images";
    list_cmd.help = "list_images";
    list_cmd.func = ListImagesCliHandler;
    esp_console_cmd_register(&detect_cmd);
    esp_console_cmd_register(&list_cmd);

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
#endif

}  // namespace

extern "C" void app_main(void) {
    ConfigureLed();

    IdentifierDetector detector;
    if (!detector.Begin()) {
        ESP_LOGE(kTag, "detector init failed");
        return;
    }

#if CONFIG_IDENTIFIER_USE_CAMERA
    static uint8_t image[kImageElementCount];
    if (!InitCameraProvider()) {
        ESP_LOGE(kTag, "camera init failed");
        return;
    }

    while (true) {
        if (!CaptureCameraImage(image)) {
            SetLed(false);
            vTaskDelay(pdMS_TO_TICKS(CONFIG_IDENTIFIER_PERIOD_MS));
            continue;
        }

        IdentifierResult result;
        const int64_t start = esp_timer_get_time();
        const bool ok = detector.Run(image, &result);
        const int64_t elapsed = esp_timer_get_time() - start;

        if (ok) {
            SetLed(result.detected);
            LogResult(result, elapsed);
        } else {
            SetLed(false);
            ESP_LOGE(kTag, "inference failed");
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_IDENTIFIER_PERIOD_MS));
    }
#else
    if (g_static_image_count == 0) {
        ESP_LOGE(kTag, "no static images available; regenerate static_image_data.cc");
        return;
    }
    ESP_LOGI(kTag, "CLI mode: use list_images or detect_image <index>");
    StartCli(&detector);
    vTaskDelay(portMAX_DELAY);
#endif
}
