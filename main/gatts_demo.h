#ifndef GATTS_DEMO_H
#define GATTS_DEMO_H

#include "esp_gatts_api.h"

#define PROFILE_A_APP_ID 0

// BLE'den alýnan JSON verisi burada tutulur
extern char g_ble_json_buf[256];
void ble_start(void);

// BLE characteristic notify için gereken yapýlar
struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

// BLE profilleri dizisi (gatts_demo.c içinde tanýmlý)
extern struct gatts_profile_inst gl_profile_tab[2];

// Characteristic özellikleri (notify özelliðini kontrol etmek için)
extern esp_gatt_char_prop_t a_property;

#endif // GATTS_DEMO_H
