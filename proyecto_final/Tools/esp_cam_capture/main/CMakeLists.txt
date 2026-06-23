idf_component_register(
    SRCS "capture_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp32-camera
    PRIV_REQUIRES nvs_flash esp_psram esp_driver_uart
)
