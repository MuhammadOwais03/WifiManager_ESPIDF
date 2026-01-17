#include "WifiManager.hpp"
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"

#define SETUP_AP_SSID "ESP32-SETUP"
#define SETUP_AP_PASS "12345678"

static const char* TAG = "WiFiManager";

WiFiManager::WiFiManager()
    : wifiEventGroup(nullptr),
      connected(false),
      server(nullptr) {}

bool WiFiManager::isConnected() const {
    return connected;
}

/* ===================== SETUP ===================== */

void WiFiManager::Setup()
{
    initWiFi();

    char ssid[32] = {};
    char pass[64] = {};

    if (loadCredentials(ssid, pass) && ssid[0])
    {
        ESP_LOGI(TAG, "Trying STA: %s", ssid);
        if (connectSTA())
        {
            connected = true;
            return;
        }
        ESP_LOGW(TAG, "STA failed → AP mode");
    }

    startConfigAP();
}

/* ===================== WIFI ===================== */

void WiFiManager::initWiFi()
{
    wifiEventGroup = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID,
        &WiFiManager::wifiEventHandlerStatic, this);

    esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP,
        &WiFiManager::wifiEventHandlerStatic, this);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

bool WiFiManager::connectSTA()
{
    char ssid[32], pass[64];
    loadCredentials(ssid, pass);

    esp_netif_create_default_wifi_sta();

    wifi_config_t cfg = {};
    strlcpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strlcpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();

    EventBits_t bits = xEventGroupWaitBits(
        wifiEventGroup,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));

    return bits & WIFI_CONNECTED_BIT;
}

/* ===================== EVENTS ===================== */

void WiFiManager::wifiEventHandlerStatic(
    void* arg, esp_event_base_t base, int32_t id, void* data)
{
    static_cast<WiFiManager*>(arg)->wifiEventHandler(base, id, data);
}

void WiFiManager::wifiEventHandler(
    esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        xEventGroupSetBits(wifiEventGroup, WIFI_FAIL_BIT);

    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        connected = true;
        xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
    }
}

/* ===================== AP MODE ===================== */

void WiFiManager::startConfigAP()
{
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap = {};
    strcpy((char*)ap.ap.ssid, SETUP_AP_SSID);
    strcpy((char*)ap.ap.password, SETUP_AP_PASS);
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.ap.max_connection = 4;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    esp_wifi_start();

    xTaskCreate(dnsServerTask, "dns", 4096, NULL, 5, NULL);
    startWebServer();
}

/* ===================== DNS ===================== */

void WiFiManager::dnsServerTask(void*)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    uint8_t buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    while (1)
    {
        int len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr*)&from, &fromlen);
        if (len < 12) continue;

        buf[2] |= 0x80;
        buf[7] = 1;

        int i = len;
        buf[i++] = 0xC0; buf[i++] = 0x0C;
        buf[i++] = 0x00; buf[i++] = 0x01;
        buf[i++] = 0x00; buf[i++] = 0x01;
        buf[i++] = 0; buf[i++] = 0; buf[i++] = 0; buf[i++] = 0;
        buf[i++] = 0; buf[i++] = 4;
        buf[i++] = 192; buf[i++] = 168; buf[i++] = 4; buf[i++] = 1;

        sendto(sock, buf, i, 0, (struct sockaddr*)&from, fromlen);
    }
}

/* ===================== HTTP ===================== */

void WiFiManager::startWebServer(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        

        httpd_uri_t save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = saveHandler};

        // 🔥 Captive portal probes
        httpd_uri_t android = {
            .uri = "/generate_204",
            .method = HTTP_GET,
            .handler = captiveHandler};

        httpd_uri_t apple = {
            .uri = "/hotspot-detect.html",
            .method = HTTP_GET,
            .handler = captiveHandler};

        httpd_uri_t windows = {
            .uri = "/connecttest.txt",
            .method = HTTP_GET,
            .handler = captiveHandler};

       

        // httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &save);

        // 🔥 REQUIRED
        httpd_register_uri_handler(server, &android);
        httpd_register_uri_handler(server, &apple);
        httpd_register_uri_handler(server, &windows);

        // httpd_register_uri_handler(server, &redirect_uri);

        ESP_LOGI(TAG, "Web server started (captive portal enabled)");
    }
}

/* ===================== HANDLERS ===================== */

esp_err_t WiFiManager::captiveHandler(httpd_req_t* req)
{
    ESP_LOGI(TAG, "Captive probe hit: %s", req->uri);

    httpd_resp_set_type(req, "text/html");
    static const char html[] =
        "<!DOCTYPE html>"
        "<html><head>"
        "<title>ESP32 WiFi Setup</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>"
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{"
        "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,sans-serif;"
        "background:linear-gradient(135deg,#1e3c72 0%,#2a5298 50%,#7e22ce 100%);"
        "min-height:100vh;"
        "display:flex;"
        "align-items:center;"
        "justify-content:center;"
        "padding:20px;"
        "position:relative;"
        "overflow:hidden"
        "}"
        "body::before{"
        "content:'';"
        "position:absolute;"
        "width:200%;"
        "height:200%;"
        "background:radial-gradient(circle,rgba(255,255,255,0.1) 1px,transparent 1px);"
        "background-size:50px 50px;"
        "animation:drift 20s linear infinite;"
        "opacity:0.3"
        "}"
        "@keyframes drift{"
        "0%{transform:translate(0,0)}"
        "100%{transform:translate(50px,50px)}"
        "}"
        ".container{"
        "background:rgba(255,255,255,0.98);"
        "backdrop-filter:blur(20px);"
        "padding:45px;"
        "border-radius:24px;"
        "box-shadow:0 25px 70px rgba(0,0,0,0.4),0 0 0 1px rgba(255,255,255,0.5) inset;"
        "max-width:420px;"
        "width:100%;"
        "animation:slideUp 0.6s cubic-bezier(0.16,1,0.3,1);"
        "position:relative;"
        "z-index:1"
        "}"
        "@keyframes slideUp{"
        "0%{opacity:0;transform:translateY(40px) scale(0.95)}"
        "100%{opacity:1;transform:translateY(0) scale(1)}"
        "}"
        ".icon-wrapper{"
        "text-align:center;"
        "margin-bottom:25px"
        "}"
        ".icon{"
        "display:inline-flex;"
        "width:64px;"
        "height:64px;"
        "background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);"
        "border-radius:16px;"
        "align-items:center;"
        "justify-content:center;"
        "font-size:32px;"
        "box-shadow:0 8px 24px rgba(102,126,234,0.3);"
        "animation:pulse 2s ease-in-out infinite"
        "}"
        "@keyframes pulse{"
        "0%,100%{transform:scale(1)}"
        "50%{transform:scale(1.05)}"
        "}"
        "h2{"
        "color:#1a202c;"
        "margin-bottom:8px;"
        "font-size:32px;"
        "font-weight:700;"
        "text-align:center;"
        "letter-spacing:-0.5px"
        "}"
        ".subtitle{"
        "color:#718096;"
        "margin-bottom:35px;"
        "font-size:15px;"
        "text-align:center;"
        "font-weight:400"
        "}"
        ".form-group{"
        "margin-bottom:28px"
        "}"
        "label{"
        "display:block;"
        "color:#2d3748;"
        "font-weight:600;"
        "margin-bottom:10px;"
        "font-size:14px;"
        "letter-spacing:0.3px"
        "}"
        "input[type='text'],input[type='password']{"
        "width:100%;"
        "padding:14px 18px;"
        "border:2px solid #e2e8f0;"
        "border-radius:12px;"
        "font-size:16px;"
        "transition:all 0.3s cubic-bezier(0.4,0,0.2,1);"
        "background:#f7fafc;"
        "color:#2d3748"
        "}"
        "input[type='text']:focus,input[type='password']:focus{"
        "outline:none;"
        "border-color:#667eea;"
        "background:#fff;"
        "box-shadow:0 0 0 4px rgba(102,126,234,0.12),"
        "0 4px 12px rgba(102,126,234,0.15);"
        "transform:translateY(-1px)"
        "}"
        "input::placeholder{"
        "color:#a0aec0"
        "}"
        "input[type='submit']{"
        "width:100%;"
        "padding:16px;"
        "background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);"
        "color:#fff;"
        "border:none;"
        "border-radius:12px;"
        "font-size:17px;"
        "font-weight:700;"
        "cursor:pointer;"
        "transition:all 0.3s cubic-bezier(0.4,0,0.2,1);"
        "box-shadow:0 6px 20px rgba(102,126,234,0.4);"
        "letter-spacing:0.5px;"
        "margin-top:10px"
        "}"
        "input[type='submit']:hover{"
        "transform:translateY(-3px);"
        "box-shadow:0 10px 28px rgba(102,126,234,0.5)"
        "}"
        "input[type='submit']:active{"
        "transform:translateY(-1px);"
        "box-shadow:0 4px 16px rgba(102,126,234,0.4)"
        "}"
        ".status-indicator{"
        "text-align:center;"
        "margin-top:25px;"
        "padding-top:25px;"
        "border-top:1px solid #e2e8f0;"
        "color:#718096;"
        "font-size:13px"
        "}"
        ".status-dot{"
        "display:inline-block;"
        "width:8px;"
        "height:8px;"
        "background:#48bb78;"
        "border-radius:50%;"
        "margin-right:6px;"
        "animation:blink 2s ease-in-out infinite"
        "}"
        "@keyframes blink{"
        "0%,100%{opacity:1}"
        "50%{opacity:0.3}"
        "}"
        "@media(max-width:480px){"
        ".container{padding:35px 28px}"
        "h2{font-size:28px}"
        ".icon{width:56px;height:56px;font-size:28px}"
        "}"
        "</style>"
        "</head><body>"
        "<div class='container'>"
        "<div class='icon-wrapper'><div class='icon'>📡</div></div>"
        "<h2>WiFi Setup</h2>"
        "<p class='subtitle'>Connect your ESP32 to your network</p>"
        "<form method='POST' action='/save'>"
        "<div class='form-group'>"
        "<label>Network Name (SSID)</label>"
        "<input type='text' name='ssid' placeholder='Enter your WiFi network name' required autocomplete='off'>"
        "</div>"
        "<div class='form-group'>"
        "<label>Password</label>"
        "<input type='password' name='password' placeholder='Enter network password' autocomplete='off'>"
        "</div>"
        "<input type='submit' value='CONNECT TO NETWORK'>"
        "</form>"
        "<div class='status-indicator'>"
        "<span class='status-dot'></span>"
        "Configuration Portal Active"
        "</div>"
        "</div>"
        "</body></html>"; // reuse your HTML
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WiFiManager::saveHandler(httpd_req_t* req)
{
    char buf[512] = {};
    httpd_req_recv(req, buf, sizeof(buf) - 1);

    char ssid[33], pass[65];
    extractParam(buf, "ssid=", ssid, sizeof(ssid));
    extractParam(buf, "password=", pass, sizeof(pass));

    WiFiManager* self =
        (WiFiManager*)req->user_ctx;

    self->saveCredentials(ssid, pass);

    httpd_resp_sendstr(req, "Saved. Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}



/* ===================== NVS ===================== */

bool WiFiManager::loadCredentials(char* ssid, char* pass)
{
    nvs_handle_t nvs;
    if (nvs_open("wifi", NVS_READONLY, &nvs) != ESP_OK) return false;

    size_t len1 = 32, len2 = 64;
    nvs_get_str(nvs, "ssid", ssid, &len1);
    nvs_get_str(nvs, "pass", pass, &len2);
    nvs_close(nvs);
    return ssid[0];
}

void WiFiManager::saveCredentials(const char* ssid, const char* pass)
{
    nvs_handle_t nvs;
    nvs_open("wifi", NVS_READWRITE, &nvs);
    nvs_set_str(nvs, "ssid", ssid);
    nvs_set_str(nvs, "pass", pass);
    nvs_commit(nvs);
    nvs_close(nvs);
}

/* ===================== UTILS ===================== */

void WiFiManager::urlDecode(const char* src, char* dst, size_t len)
{
    size_t i = 0, j = 0;
    while (src[i] && j < len - 1)
    {
        if (src[i] == '+') dst[j++] = ' ';
        else if (src[i] == '%' && isxdigit(src[i+1]) && isxdigit(src[i+2]))
        {
            char hex[3] = {src[i+1], src[i+2], 0};
            dst[j++] = strtol(hex, NULL, 16);
            i += 2;
        }
        else dst[j++] = src[i];
        i++;
    }
    dst[j] = 0;
}

void WiFiManager::extractParam(
    const char* buf, const char* key,
    char* out, size_t size)
{
    const char* p = strstr(buf, key);
    if (!p) { out[0] = 0; return; }
    p += strlen(key);
    const char* e = strchr(p, '&');
    size_t len = e ? e - p : strlen(p);
    if (len >= size) len = size - 1;
    char tmp[128];
    memcpy(tmp, p, len);
    tmp[len] = 0;
    urlDecode(tmp, out, size);
}
