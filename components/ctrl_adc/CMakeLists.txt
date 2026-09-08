idf_component_register(
    SRCS "ctrl_adc.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc esp_timer log freertos
)