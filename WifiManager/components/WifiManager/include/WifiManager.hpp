#pragma once

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"

class WiFiManager {
public:
    WiFiManager();

    void Setup();
    bool isConnected() const;

private:
    static constexpr int WIFI_CONNECTED_BIT = BIT0;
    static constexpr int WIFI_FAIL_BIT      = BIT1;

    EventGroupHandle_t wifiEventGroup;
    bool connected;

    httpd_handle_t server;

    /* ===== Core ===== */
    void initWiFi();
    bool connectSTA();
    void startConfigAP();

    /* ===== Events ===== */
    static void wifiEventHandlerStatic(
        void* arg, esp_event_base_t base, int32_t id, void* data);
    void wifiEventHandler(
        esp_event_base_t base, int32_t id, void* data);

    /* ===== DNS ===== */
    static void dnsServerTask(void* pv);

    /* ===== NVS ===== */
    bool loadCredentials(char* ssid, char* pass);
    void saveCredentials(const char* ssid, const char* pass);

    /* ===== HTTP ===== */
    void startWebServer();
    static esp_err_t captiveHandler(httpd_req_t* req);
    static esp_err_t saveHandler(httpd_req_t* req);

    /* ===== Utils ===== */
    static void urlDecode(const char* src, char* dest, size_t len);
    static void extractParam(const char* buf, const char* key,
                             char* out, size_t out_size);
};
