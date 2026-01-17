Here’s a **clean, professional `README.md`** you can drop straight into your GitHub repo.
It’s written like a **real ESP-IDF production project**, not a tutorial toy.

---

# ESP32 WiFi Manager (Captive Portal)

A **robust WiFi provisioning manager for ESP32 (ESP-IDF)** that automatically connects to saved WiFi credentials and falls back to an **Access Point with captive portal** when credentials are missing or invalid.

Designed for **real-world IoT devices**, similar to how smart plugs, bulbs, and routers handle WiFi onboarding.

---

## ✨ Features

* 🔁 **Auto STA → AP fallback**
* 📡 **Captive Portal (opens automatically on phones & laptops)**
* 🌐 **DNS redirect (catch-all)**
* 💾 **WiFi credentials stored securely in NVS**
* 🔐 WPA2 Access Point
* ⚙️ Written in **C++ (ESP-IDF style)**
* 🧩 Clean **WiFiManager class abstraction**
* 🧠 OS-aware captive portal support (Android / iOS / Windows)

---

## 📂 Project Structure

```
.
├── CMakeLists.txt
├── README.md
└── main/
    ├── CMakeLists.txt
    ├── main.cpp
    ├── WiFiManager.h
    └── WiFiManager.cpp
```

---

## 🔄 Connection Flow

```text
Boot
 └─► Load WiFi credentials from NVS
      ├─► Credentials valid → Connect to WiFi (STA)
      └─► Missing / invalid
            └─► Start AP + DNS + Web Server
                  └─► Captive portal opens automatically
                        └─► User enters WiFi credentials
                              └─► Credentials saved to NVS
                                    └─► Device connects to WiFi
```

---

## 📡 Access Point Details

| Setting     | Value                |
| ----------- | -------------------- |
| SSID        | `ESP32-SETUP`        |
| IP Address  | `192.168.4.1`        |
| Captive URL | `http://192.168.4.1` |
| Auth Mode   | WPA2-PSK             |

> ℹ️ Captive portal automatically opens on most devices.
> If it doesn’t, manually open `http://192.168.4.1`.

---

## 🧪 Tested On

* ✅ Android (Chrome / System captive portal)
* ✅ Windows 10 / 11
* ✅ Linux
* ✅ ESP-IDF v5.x
* ✅ ESP32 / ESP32-S3

---

## 🛠️ Build & Flash

### 1️⃣ Set up ESP-IDF

```bash
. $HOME/esp/esp-idf/export.sh
```

### 2️⃣ Configure target

```bash
idf.py set-target esp32
```

### 3️⃣ Build & flash

```bash
idf.py build flash monitor
```

---

## 🚀 Usage Example

### `main.cpp`

```cpp
WiFiManager wifi;
wifi.Setup();

if (wifi.isConnected()) {
    // Safe to start MQTT / HTTP client / OTA
}
```

---

## 🧠 Design Notes

* DNS server responds to **all domain queries** with `192.168.4.1`
* HTTP server registers a wildcard URI (`*`) for redirection
* OS captive portal detection URLs are handled automatically
* WiFi credentials persist across reboots using NVS

---

## 🔒 Security Notes

* Credentials are stored locally in ESP32 NVS
* No cloud dependency
* AP password can be customized or disabled for open setup mode
* HTTPS can be added for production use

---

## 🧩 Integration

This WiFiManager works perfectly with:

* MQTT clients
* OTA updates
* REST APIs
* WebSockets
* Home automation devices



## 🤝 Contributing

Pull requests are welcome.
If you find a bug or want to add a feature, feel free to open an issue.


## 🙌 Author

**Muhammad Owais**
ESP-IDF | Embedded Systems | IoT | Backend
GitHub: https://github.com/MuhammadOwais03



