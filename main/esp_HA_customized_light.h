#ifndef ESP_HA_CUSTOMIZED_LIGHT_H
#define ESP_HA_CUSTOMIZED_LIGHT_H

#include "esp_zigbee_core.h"
#include "light_driver.h"

/* Zigbee configuration */
#define MAX_CHILDREN                    10          // Maksimum baðlý cihaz sayýsý
#define INSTALLCODE_POLICY_ENABLE       false       // Güvenlik için kurulum kodu politikasý
#define HA_ESP_LIGHT_ENDPOINT           10          // Light cihazýnýn endpoint numarasý
#define ESP_ZB_PRIMARY_CHANNEL_MASK     (1l << 13)  // Zigbee ana kanal maskesi (13. kanal)

/* Üretici kimliði bilgileri */
#define ESP_MANUFACTURER_NAME "\x09""ESPRESSIF"
#define ESP_MODEL_IDENTIFIER   "\x07"CONFIG_IDF_TARGET

/* Zigbee Coordinator (ZC) konfigürasyonu makrosu */
#define ESP_ZB_ZC_CONFIG()                                                              \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,                                  \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

/* Varsayýlan Zigbee donaným (radio) yapýlandýrmasý */
#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

/* Varsayýlan Zigbee host yapýlandýrmasý */
#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

/* Zigbee baþlatma fonksiyonu */
void zigbee_start(void);
void zb_send_payload_from_ble(const char *json_str);

#endif // ESP_HA_CUSTOMIZED_LIGHT_H
