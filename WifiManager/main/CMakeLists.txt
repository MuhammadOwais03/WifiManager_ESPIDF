idf_component_register(SRCS "main.cpp"
                    REQUIRES esp_wifi nvs_flash esp_netif esp_http_server WifiManager
                    
                    INCLUDE_DIRS ".")
