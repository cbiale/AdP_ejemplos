idf_component_register(SRCS "wifi.c" "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash esp_netif driver esp_wifi esp_event esp_timer)
