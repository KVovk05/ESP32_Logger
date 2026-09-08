idf_component_register(
    SRCS "oled_display.c" "u8g2_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver i2c_bus rtc_ds1307 esp_timer log freertos
)