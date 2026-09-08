idf_component_register(
    SRCS "wifi_prov.c"
    INCLUDE_DIRS "include"
    REQUIRES wifi_provisioning esp_wifi esp_netif esp_event nvs_flash log freertos
)