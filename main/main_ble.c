#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "BT_SAMPLE";

#define DEVICE_NAME "ESP32-S3-BT"

// Nordic UART Service UUIDs
#define NUS_SERVICE_UUID        0x0001
#define NUS_CHAR_TX_UUID        0x0003
#define NUS_CHAR_RX_UUID        0x0002

static uint8_t nus_service_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E,
};

static uint8_t nus_char_tx_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E,
};

static uint8_t nus_char_rx_uuid[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E,
};

#define NUS_NUM_HANDLE 6

enum {
    NUS_IDX_SVC,
    NUS_IDX_TX_CHAR,
    NUS_IDX_TX_VAL,
    NUS_IDX_TX_CFG,
    NUS_IDX_RX_CHAR,
    NUS_IDX_RX_VAL,
    NUS_IDX_NB,
};

static uint16_t nus_handle_table[NUS_IDX_NB];
static uint16_t nus_conn_id = 0;
static uint16_t nus_mtu_size = 23;
static bool nus_notify_enabled = false;
static esp_gatt_if_t nus_gatts_if = ESP_GATT_IF_NONE;

// Required UUIDs and properties for GATT
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;

#define CHAR_DECLARATION_SIZE 1

static uint8_t adv_config_done = 0;
#define adv_config_flag      (1 << 0)
#define scan_rsp_config_flag (1 << 1)

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint8_t adv_service_uuid128[16] = {
    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
    .set_scan_rsp = true,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 0,
    .p_service_uuid = NULL,
    .flag = 0,
};

// Nordic UART Service attribute table
static const esp_gatts_attr_db_t nus_gatt_db[NUS_IDX_NB] =
{
    // Service Declaration
    [NUS_IDX_SVC] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(nus_service_uuid), sizeof(nus_service_uuid), (uint8_t *)nus_service_uuid}},

    // TX Characteristic Declaration
    [NUS_IDX_TX_CHAR] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify}},

    // TX Characteristic Value
    [NUS_IDX_TX_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)nus_char_tx_uuid, ESP_GATT_PERM_READ,
      500, 0, NULL}},

    // TX Characteristic - Client Characteristic Configuration Descriptor
    [NUS_IDX_TX_CFG] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      sizeof(uint16_t), 0, NULL}},

    // RX Characteristic Declaration
    [NUS_IDX_RX_CHAR] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE, CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_write}},

    // RX Characteristic Value
    [NUS_IDX_RX_VAL] =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_128, (uint8_t *)nus_char_rx_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      500, 0, NULL}},
};

// Function to send data via NUS
void nus_send_data(uint8_t *data, uint16_t length)
{
    if (nus_notify_enabled && nus_gatts_if != ESP_GATT_IF_NONE && length <= (nus_mtu_size - 3)) {
        esp_err_t ret = esp_ble_gatts_send_indicate(nus_gatts_if, nus_conn_id, nus_handle_table[NUS_IDX_TX_VAL],
                                                   length, data, false);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "NUS TX: %.*s", length, (char*)data);
        } else {
            ESP_LOGE(TAG, "Failed to send NUS data: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "NUS not ready for sending data (enabled=%d, gatts_if=%d, len=%d)",
                 nus_notify_enabled, nus_gatts_if, length);
    }
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~adv_config_flag);
        if (adv_config_done == 0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        ESP_LOGI(TAG, "Advertising data set complete");
        break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        adv_config_done &= (~scan_rsp_config_flag);
        if (adv_config_done == 0){
            esp_ble_gap_start_advertising(&adv_params);
        }
        ESP_LOGI(TAG, "Scan response data set complete");
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Advertising started successfully");
        } else {
            ESP_LOGE(TAG, "Failed to start advertising");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Advertising stopped successfully");
        } else {
            ESP_LOGE(TAG, "Failed to stop advertising");
        }
        break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "Security request received");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "Numeric comparison request - Just Works pairing");
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        break;

    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
        ESP_LOGI(TAG, "Passkey request - Just Works pairing");
        esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 0);
        break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "Authentication successful - Just Works pairing completed");
        } else {
            ESP_LOGE(TAG, "Authentication failed, reason: %x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;

    default:
        ESP_LOGD(TAG, "GAP event: %d", event);
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "GATT server registered, app_id %d, gatts_if %d", param->reg.app_id, gatts_if);
        nus_gatts_if = gatts_if;

        esp_err_t ret = esp_ble_gap_set_device_name(DEVICE_NAME);
        if (ret) {
            ESP_LOGE(TAG, "Failed to set device name: %s", esp_err_to_name(ret));
        }

        ret = esp_ble_gap_config_adv_data(&adv_data);
        if (ret) {
            ESP_LOGE(TAG, "Failed to config advertising data: %s", esp_err_to_name(ret));
        } else {
            adv_config_done |= adv_config_flag;
        }

        ret = esp_ble_gap_config_adv_data(&scan_rsp_data);
        if (ret) {
            ESP_LOGE(TAG, "Failed to config scan response data: %s", esp_err_to_name(ret));
        } else {
            adv_config_done |= scan_rsp_config_flag;
        }

        esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_ONLY;
        esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
        uint8_t key_size = 16;
        uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
        esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
        esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

        // Create Nordic UART Service attribute table
        ret = esp_ble_gatts_create_attr_tab(nus_gatt_db, gatts_if, NUS_IDX_NB, 0);
        if (ret) {
            ESP_LOGE(TAG, "Failed to create NUS attribute table: %s", esp_err_to_name(ret));
        }
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "Create attribute table failed, error code = %x", param->add_attr_tab.status);
        } else if (param->add_attr_tab.num_handle != NUS_IDX_NB) {
            ESP_LOGE(TAG, "Create attribute table abnormally, num_handle (%d) doesn't equal to NUS_IDX_NB(%d)",
                     param->add_attr_tab.num_handle, NUS_IDX_NB);
        } else {
            ESP_LOGI(TAG, "Nordic UART Service attribute table created successfully");
            memcpy(nus_handle_table, param->add_attr_tab.handles, sizeof(nus_handle_table));
            esp_ble_gatts_start_service(nus_handle_table[NUS_IDX_SVC]);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Device connected, conn_id = %d", param->connect.conn_id);
        nus_conn_id = param->connect.conn_id;
        nus_notify_enabled = false;

        // Update connection parameters
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;     // timeout = 400*10ms = 4000ms
        esp_ble_gap_update_conn_params(&conn_params);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Device disconnected, restarting advertising");
        nus_notify_enabled = false;
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(TAG, "MTU exchange, MTU = %d", param->mtu.mtu);
        nus_mtu_size = param->mtu.mtu;
        break;

    case ESP_GATTS_WRITE_EVT:
        ESP_LOGI(TAG, "GATT write event: handle=%d, len=%d, need_rsp=%d",
                 param->write.handle, param->write.len, param->write.need_rsp);

        if (!param->write.is_prep) {
            // Handle NUS RX characteristic write (data received from client)
            if (param->write.handle == nus_handle_table[NUS_IDX_RX_VAL]) {
                ESP_LOGI(TAG, "NUS RX: %.*s", param->write.len, (char*)param->write.value);

                // Echo received data back to client (delayed to avoid response conflict)
                vTaskDelay(pdMS_TO_TICKS(10));
                char response[64];
                int len = snprintf(response, sizeof(response), "Echo: %.*s", param->write.len, (char*)param->write.value);
                nus_send_data((uint8_t*)response, len);
            }
            // Handle TX characteristic configuration (notification enable/disable)
            else if (param->write.handle == nus_handle_table[NUS_IDX_TX_CFG]) {
                uint16_t descr_value = param->write.value[1]<<8 | param->write.value[0];
                if (descr_value == 0x0001) {
                    ESP_LOGI(TAG, "NUS notifications enabled");
                    nus_notify_enabled = true;

                    // Send welcome message (delayed to avoid response conflict)
                    vTaskDelay(pdMS_TO_TICKS(100));
                    char welcome[] = "ESP32-S3 Nordic UART Service Ready!";
                    nus_send_data((uint8_t*)welcome, strlen(welcome));
                } else if (descr_value == 0x0000) {
                    ESP_LOGI(TAG, "NUS notifications disabled");
                    nus_notify_enabled = false;
                }
            }
        }

        // Send response only if required
        if (param->write.need_rsp) {
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;

    case ESP_GATTS_READ_EVT:
        ESP_LOGI(TAG, "GATT read event: handle=%d", param->read.handle);
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 0;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
        break;

    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(TAG, "GATT execute write event");
        esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, NULL);
        break;

    default:
        ESP_LOGD(TAG, "GATTS event: %d", event);
        break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 Bluetooth Sample Starting (Just Works pairing)");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Bluetooth controller initialized");

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Bluetooth controller enabled");

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Bluedroid initialized");

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Bluedroid enabled");

    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        ESP_LOGE(TAG, "GAP register callback failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "GAP callback registered");

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        ESP_LOGE(TAG, "GATTS register callback failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "GATTS callback registered");

    ret = esp_ble_gatts_app_register(0);
    if (ret){
        ESP_LOGE(TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "GATTS app registered");

    ESP_LOGI(TAG, "Bluetooth device is ready and advertising as '%s' (Just Works pairing)", DEVICE_NAME);
}