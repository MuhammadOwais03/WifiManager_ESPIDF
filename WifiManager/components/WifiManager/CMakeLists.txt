idf_component_register(SRCS "WifiManager.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi nvs_flash esp_netif esp_http_server
                    )
