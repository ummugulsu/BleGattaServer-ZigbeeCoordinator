#include "gatts_demo.h"
#include "esp_HA_customized_light.h"
#include "nvs_flash.h"
#include "esp_err.h"



void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ble_start();     // BLE baþlat
    zigbee_start();  // Zigbee baþlat
}
