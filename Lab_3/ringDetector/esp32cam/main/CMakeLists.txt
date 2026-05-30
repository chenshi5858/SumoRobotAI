idf_component_register(
    SRCS
        "main.c"
        "camera_app.c"
        "espnow_comm.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_timer
        espressif__esp32-camera
)
