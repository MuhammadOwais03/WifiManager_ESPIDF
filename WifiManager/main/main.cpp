
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "WifiManager.hpp"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "System booting...");

    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS issue detected, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    
    WiFiManager wifiManager;
    wifiManager.Setup();

    
    while (true)
    {
        if (wifiManager.isConnected()) {
            ESP_LOGI(TAG, "WiFi connected");
        } else {
            ESP_LOGI(TAG, "WiFi not connected (AP or retrying)");
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}



