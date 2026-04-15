/*
 * Ejemplo: GATT server NimBLE que recibe comandos de movimiento y controla motores GPIO
 * Basado en el proyecto original de Ejercicio_5 (robot_audio), pero reemplaza la lógica
 * de audio por un servicio BLE que recibe comandos desde la web.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

/* NimBLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "EJ1_BLE_MOTORES";

/* ============== CONFIG ============== */
// Motores (ajusta a tu conexión real)
#define MOTOR_A_PIN1 6
#define MOTOR_A_PIN2 7
#define MOTOR_B_PIN1 16
#define MOTOR_B_PIN2 17
#define MOTOR_A_ENA  9
#define MOTOR_B_ENB  10

/* ============== MOTORES ============== */
void init_motors(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask =
            (1ULL << MOTOR_A_PIN1) |
            (1ULL << MOTOR_A_PIN2) |
            (1ULL << MOTOR_B_PIN1) |
            (1ULL << MOTOR_B_PIN2) |
            (1ULL << MOTOR_A_ENA)  |
            (1ULL << MOTOR_B_ENB),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    gpio_set_level(MOTOR_A_ENA, 1);
    gpio_set_level(MOTOR_B_ENB, 1);
}

void move_robot(const char* cmd) {
    if (cmd == NULL) return;
    if (strcasecmp(cmd, "forward") == 0 || strcasecmp(cmd, "f") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
        ESP_LOGI(TAG, "MOVE FORWARD");
    } else if (strcasecmp(cmd, "back") == 0 || strcasecmp(cmd, "backward") == 0 || strcasecmp(cmd, "b") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 1);
        ESP_LOGI(TAG, "MOVE BACKWARD");
    } else if (strcasecmp(cmd, "left") == 0 || strcasecmp(cmd, "l") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 1);
        gpio_set_level(MOTOR_B_PIN1, 1); gpio_set_level(MOTOR_B_PIN2, 0);
        ESP_LOGI(TAG, "MOVE LEFT");
    } else if (strcasecmp(cmd, "right") == 0 || strcasecmp(cmd, "r") == 0) {
        gpio_set_level(MOTOR_A_PIN1, 1); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 1);
        ESP_LOGI(TAG, "MOVE RIGHT");
    } else {
        gpio_set_level(MOTOR_A_PIN1, 0); gpio_set_level(MOTOR_A_PIN2, 0);
        gpio_set_level(MOTOR_B_PIN1, 0); gpio_set_level(MOTOR_B_PIN2, 0);
        ESP_LOGI(TAG, "STOP");
    }
}

/* ============== BLE GATT ============== */
static const char *device_name = "RobotCar";

/* Characteristic write handler */
static int gatt_chr_write_cb(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        int len = OS_MBUF_PKTLEN(ctxt->om);
        if (len > 0 && len < 128) {
            char buf[128];
            int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf)-1, NULL);
            if (rc == 0) {
                buf[len] = '\0';
                // Trim whitespace
                for (int i = 0; i < len; i++) if (buf[i] == '\r' || buf[i] == '\n') buf[i] = '\0';
                ESP_LOGI(TAG, "BLE RX: %s", buf);
                move_robot(buf);
            }
        }
    }
    return 0; /* success */
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0xFFE0),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(0xFFE1),
                .flags = BLE_GATT_CHR_F_WRITE,
                .access_cb = gatt_chr_write_cb,
            },
            { 0 }
        },
    },
    { 0 },
};

static void ble_app_advertise(void) {
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_BREDR_UNSUP | BLE_HS_ADV_F_DISC_GEN;
    fields.tx_pwr_lvl_is_present = 1;
    fields.name = (uint8_t*)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting adv fields: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    if (rc != 0) ESP_LOGE(TAG, "error enabling adv: %d", rc);
}

static void ble_app_on_sync(void) {
    int rc;
    rc = ble_hs_id_infer_auto(0, NULL);
    if (rc != 0) ESP_LOGE(TAG, "ble_hs_id_infer_auto rc=%d", rc);

    ble_svc_gap_device_name_set(device_name);

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) ESP_LOGE(TAG, "ble_gatts_count_cfg rc=%d", rc);

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) ESP_LOGE(TAG, "ble_gatts_add_svcs rc=%d", rc);

    rc = ble_gatts_start();
    if (rc != 0) ESP_LOGE(TAG, "ble_gatts_start rc=%d", rc);

    ble_app_advertise();
}

static void ble_app_on_reset(int reason) {
    ESP_LOGI(TAG, "BLE reset; reason=%d", reason);
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void) {
    ESP_LOGI(TAG, "Inicio firmware BLE - control motores");

    // Init NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    init_motors();

    // Initialize NimBLE HCI transport
    ESP_ERROR_CHECK(esp_nimble_hci_init());
    nimble_port_init();

    ble_hs_cfg.reset_cb = ble_app_on_reset;
    ble_hs_cfg.sync_cb = ble_app_on_sync;

    nimble_port_freertos_init(ble_host_task);

    // The rest is handled by callbacks and GATT write handler
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
