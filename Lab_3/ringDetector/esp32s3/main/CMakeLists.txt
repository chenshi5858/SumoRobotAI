idf_component_register(
    SRCS
        "main.c"
        "espnow_receiver.c"
        "motor_control.c"
        "ring_logic.c"
        "robot_state.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_timer
        driver
        esp_driver_gpio
        esp_driver_ledc
)
