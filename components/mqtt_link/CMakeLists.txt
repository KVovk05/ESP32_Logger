idf_component_register(
    SRCS "mqtt_link.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp_netif lwip log freertos
)