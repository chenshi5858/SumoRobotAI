#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main_functions.h"
#include "lab3_tfmicro_config.h"

extern "C" void app_main(void)
{
    setup();

    while (true) {
        loop();
        vTaskDelay(pdMS_TO_TICKS(OUTPUT_PERIOD_MS));
    }
}
