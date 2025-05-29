/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee customized server Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_HA_customized_light.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "gatts_demo.h"  // g_ble_json_buf erişimi
 // main.c veya esp_HA_customized_light.c en üstüne ekleyin:
uint16_t g_last_zigbee_sender_short_addr = 0;

static const char *TAG = "ESP_HA_ON_OFF_LIGHT";

static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;
    if (!is_inited) {
        light_driver_init(LIGHT_DEFAULT_OFF);
        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee bdb commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{

    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;
    esp_zb_zdo_signal_leave_indication_params_t *leave_ind_params = NULL;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Active Zigbee channel: %d", esp_zb_get_current_channel());

            ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network formation");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                esp_zb_bdb_open_network(180);
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Formed network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGI(TAG, "Restart network formation (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering started");
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        g_last_zigbee_sender_short_addr = dev_annce_params->device_short_addr;  // << ADRESİ KAYDET
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE_INDICATION:
        leave_ind_params = (esp_zb_zdo_signal_leave_indication_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        if (!leave_ind_params->rejoin) {
            esp_zb_ieee_addr_t leave_addr;
            memcpy(leave_addr, leave_ind_params->device_addr, sizeof(esp_zb_ieee_addr_t));
            ESP_LOGI(TAG, "Zigbee Node is leaving network: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
                     leave_addr[7], leave_addr[6], leave_addr[5], leave_addr[4],
                     leave_addr[3], leave_addr[2], leave_addr[1], leave_addr[0]);
        }
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

/*

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    bool light_state = 0;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state;
                ESP_LOGI(TAG, "Light sets to %s", light_state ? "On" : "Off");
                light_driver_set_power(light_state);
            }
        }
    }
    return ret;
}


bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind) {
    if (ind.cluster_id == 0x0230) {
        char payload[64] = { 0 };
        memcpy(payload, ind.asdu, ind.asdu_length);
        payload[ind.asdu_length] = '\0';

        ESP_LOGI(TAG, "Gelen Zigbee komutu: %s", payload);

        const char* yanit_durumu = NULL;

        if (strcmp(payload, "ON") == 0) {
            light_driver_set_power(true);
            yanit_durumu = "ON";
            ESP_LOGI(TAG, "Işık açıldı");
        }
        else if (strcmp(payload, "OFF") == 0) {
            light_driver_set_power(false);
            yanit_durumu = "OFF";
            ESP_LOGI(TAG, "Işık kapatıldı");
        }
        else {
            ESP_LOGW(TAG, "Bilinmeyen komut: %s", payload);
            return true;
        }

        esp_zb_apsde_data_req_t yanit_req = {
            .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
           .dst_addr.addr_short = ind.src_short_addr,  // ✅ Doğru kullanım
            .dst_endpoint =1,
            .profile_id = 0x0104,
            .cluster_id = 0x0230,
            .src_endpoint = HA_ESP_LIGHT_ENDPOINT,
            .asdu = (uint8_t*)yanit_durumu,
            .asdu_length = strlen(yanit_durumu),
            .tx_options = ESP_ZB_APSDE_TX_OPT_ACK_TX,
            .radius = 1,
            .use_alias = false,
        };

        esp_err_t err = esp_zb_aps_data_request(&yanit_req);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Yanıt olarak gönderildi: %s", yanit_durumu);
        }
        else {
            ESP_LOGE(TAG, "Yanıt gönderilemedi! Hata: %s", esp_err_to_name(err));
        }

        return true;
    }

    return false;
}
*/

bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind) {
    if (ind.cluster_id == 0x0230) {
        char payload[64] = { 0 };
        memcpy(payload, ind.asdu, ind.asdu_length);
        payload[ind.asdu_length] = '\0';

        ESP_LOGI("ZIGBEE→LIGHT", "Gelen Zigbee komutu: %s", payload);

        // BLE üzerinden Android'e gönder (notify)
        if (a_property & ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
            esp_ble_gatts_send_indicate(
                gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
                gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                strlen(payload),
                (uint8_t*)payload,
                false
            );

        }

        // JSON parse
        cJSON* json = cJSON_Parse(payload);
        if (json == NULL) {
            ESP_LOGE("ZIGBEE→DEVICE", "Geçersiz JSON!");
            return true;
        }

        // İlk ve tek key-value çiftine ulaş
        cJSON* item = json->child;
        if (!item) {
            ESP_LOGW("ZB_RECEIVE", "JSON boş.");
            cJSON_Delete(json);
            return true;
        }
        // Tip bağımsız olarak value'yu string haline getir
        char value_str[64] = { 0 };

        if (cJSON_IsString(item)) {
            snprintf(value_str, sizeof(value_str), "%s", item->valuestring);
        }
        else if (cJSON_IsNumber(item)) {
            snprintf(value_str, sizeof(value_str), "%.2f", item->valuedouble);
        }
        else if (cJSON_IsBool(item)) {
            snprintf(value_str, sizeof(value_str), "%s", (item->type == cJSON_True) ? "true" : "false");
        }
        else {
            snprintf(value_str, sizeof(value_str), "unknown");
        }

        ESP_LOGI("ZB_RECEIVE", "VALUE: %s", value_str);

        // Işık kontrolü (değer metin olarak karşılaştırılır)
        if (strcmp(value_str, "ON") == 0 || strcmp(value_str, "true") == 0 || strcmp(value_str, "1.00") == 0) {
            ESP_LOGI("ZB_RECEIVE", "Işık açılıyor");
            light_driver_set_power(true);
        }
        else if (strcmp(value_str, "OFF") == 0 || strcmp(value_str, "false") == 0 || strcmp(value_str, "0.00") == 0) {
            ESP_LOGI("ZB_RECEIVE", "Işık kapatılıyor");
            light_driver_set_power(false);
        }
        else {
            ESP_LOGW("ZB_RECEIVE", "Değer tanınmadı: %s", value_str);
        }


        cJSON_Delete(json);
        return true;
    }

    return false;
}




void zb_send_payload_from_ble(const char* json_str) {
    ESP_LOGI("BLE→ZIGBEE", "Received JSON from BLE: %s", json_str);

    cJSON* json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE("BLE→ZIGBEE", "Invalid JSON!");
        return;
    }

  
        esp_zb_apsde_data_req_t req = {
            .dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .dst_addr.addr_short = g_last_zigbee_sender_short_addr, // End device adresi
            .dst_endpoint = 1,
            .profile_id = 0x0104,
            .cluster_id = 0x0230,  // özel cluster ID
            .src_endpoint = 10,
            .asdu = (uint8_t*)json_str,
            .asdu_length = strlen(json_str),
            .tx_options = ESP_ZB_APSDE_TX_OPT_ACK_TX,
            .radius = 1,
            .use_alias = false,
        };

        ESP_LOGI("BLE→ZIGBEE", "Sending APS payload: %s", json_str);
        esp_zb_aps_data_request(&req);
   

    cJSON_Delete(json);
}



/*
bool zb_apsde_data_indication_handler(esp_zb_apsde_data_ind_t ind)
{
    if (ind.cluster_id == 0x0230) {
        ESP_LOGI(TAG, "Özel APS mesajı alındı: %.*s", (int)ind.asdu_length, ind.asdu);

        // Gelen veri "ON" mu?
        if (strncmp((const char*)ind.asdu, "ON", ind.asdu_length) == 0) {
            ESP_LOGI(TAG, "Komut: ON → ışık açılıyor");
            light_driver_set_power(true);
        }

        // Gelen veri "OFF" mu?
        else if (strncmp((const char*)ind.asdu, "OFF", ind.asdu_length) == 0) {
            ESP_LOGI(TAG, "Komut: OFF → ışık kapatılıyor");
            light_driver_set_power(false);
        }

        // Bilinmeyen komut
        else {
            ESP_LOGW(TAG, "Bilinmeyen komut alındı: %.*s", (int)ind.asdu_length, ind.asdu);
        }

        return true; // mesaj işlendi
    }

    return false; // mesaj başka handler’a geçsin
}
*/


/*
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

*/


static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack with Zigbee coordinator config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    uint8_t test_attr, test_attr2;
    test_attr = 0;
    test_attr2 = 3;
    /* basic cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &test_attr);
    esp_zb_cluster_update_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr2);
    /* identify cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &test_attr);
    /* group cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_GROUPS);
    esp_zb_groups_cluster_add_attr(esp_zb_groups_cluster, ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID, &test_attr);
    /* scenes cluster create with standard cluster + customized */
    esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(NULL);
    esp_zb_cluster_update_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID, &test_attr);
    /* on-off cluster create with standard cluster config*/
    esp_zb_on_off_cluster_cfg_t on_off_cfg;
    on_off_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    /* update basic cluster in the existed cluster list */
    esp_zb_cluster_list_update_cluster(esp_zb_cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_ID_BASIC,
                                       ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    /* add created endpoint (cluster_list) to endpoint list */
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = HA_ESP_LIGHT_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, endpoint_config);
    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_aps_data_indication_handler_register(zb_apsde_data_indication_handler);

    //esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

void zigbee_start(void) 
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    //ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
