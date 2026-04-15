idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "${IDF_PATH}/components/bt/host/nimble/esp-hci/include" "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/transport/include" "${IDF_PATH}/components/bt/porting_btdm/controller/ble/include" "${IDF_PATH}/components/bt/porting/include" "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/include" "${IDF_PATH}/components/bt/porting/npl/freertos/include" "${IDF_PATH}/components/bt/porting_btdm/controller/btdm_common/include"
    PRIV_REQUIRES bt nvs_flash driver esp_driver_gpio
)
